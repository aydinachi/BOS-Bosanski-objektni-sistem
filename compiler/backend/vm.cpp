#include "backend/vm.hpp"
#include "lexer/lexer.hpp"

#include <cmath>
#include <sstream>

namespace bos {

VirtualMachine::VirtualMachine(const VMOptions& options)
    : options_(options), stdlib_(options.sandboxDir) {}

Value VirtualMachine::constantToValue(const Constant& c) const {
    switch (c.kind) {
        case ConstKind::Prazno: return Value::prazno();
        case ConstKind::Cijeli: return Value::cijeli(c.intVal);
        case ConstKind::Decimalni: return Value::decimalni(c.floatVal);
        case ConstKind::Logicki: return Value::logicki(c.boolVal);
        case ConstKind::Znak: return Value::znak(c.charVal);
        case ConstKind::Tekst: return Value::tekst(c.strVal);
    }
    return Value::prazno();
}

Value VirtualMachine::defaultValueForTag(uint8_t tag) const {
    switch (tag) {
        case 1: return Value::cijeli(0);
        case 2: return Value::decimalni(0.0);
        case 3: return Value::tekst("");
        case 4: return Value::znak(0);
        case 5: return Value::logicki(false);
        case 6: return Value::niz();
        case 7: return Value::lista();
        case 8: return Value::rjecnik();
        default: return Value::prazno();
    }
}

Value VirtualMachine::pop() {
    if (stack_.empty()) {
        throw BosRuntimeException("Interna greška VM steka (prazan stek).", 500);
    }
    Value v = std::move(stack_.back());
    stack_.pop_back();
    return v;
}

bool VirtualMachine::dispatchException(const Value& errVal,
                                       std::vector<CallFrame>& frames,
                                       std::vector<TryHandler>& handlers) {
    if (handlers.empty()) return false;
    TryHandler h = handlers.back();
    handlers.pop_back();

    while (frames.size() > h.frameDepth) {
        frames.pop_back();
    }
    while (stack_.size() > h.stackDepth) {
        stack_.pop_back();
    }
    frames.back().ip = h.catchIp;
    stack_.push_back(errVal);
    return true;
}

VMResult VirtualMachine::execute(const BytecodeModule& module, DiagnosticEngine& diagnostics) {
    VMResult result;
    if (module.functions.empty()) {
        return result;
    }

    stack_.clear();
    globals_.assign(module.globalNames.size(), Value::prazno());

    std::vector<CallFrame> frames;
    std::vector<TryHandler> tryHandlers;

    CallFrame mainFrame;
    mainFrame.funcIndex = 0;
    mainFrame.ip = 0;
    mainFrame.locals.assign(static_cast<size_t>(module.functions[0].localCount), Value::prazno());
    frames.push_back(std::move(mainFrame));

    uint64_t steps = 0;

    while (!frames.empty()) {
        CallFrame& frame = frames.back();
        const FunctionIR& fn = module.functions[frame.funcIndex];

        if (frame.ip >= fn.code.size()) {
            frames.pop_back();
            continue;
        }

        if (++steps > options_.maxSteps) {
            result.ok = false;
            result.stepsExecuted = steps;
            result.errorLine = fn.code[frame.ip].line;
            result.runtimeError = "Prekoračen maksimalan broj instrukcija (" + std::to_string(options_.maxSteps)
                + "). Moguća beskonačna petlja.";
            diagnostics.error(DiagnosticStage::Runtime,
                               SourceLocation{module.filename, result.errorLine, 1},
                               result.runtimeError, "R999");
            return result;
        }

        const Instruction& ins = fn.code[frame.ip++];
        int32_t curLine = ins.line;

        try {
            switch (ins.op) {
                case OpCode::PUSH_CONST:
                    stack_.push_back(constantToValue(fn.constants[ins.a]));
                    break;

                case OpCode::PUSH_PRAZNO:
                    stack_.push_back(Value::prazno());
                    break;

                case OpCode::POP:
                    if (!stack_.empty()) stack_.pop_back();
                    break;

                case OpCode::DUP:
                    if (stack_.empty()) throw BosRuntimeException("Stek prazan za DUP.", 500);
                    stack_.push_back(stack_.back());
                    break;

                case OpCode::LOAD_LOCAL:
                    stack_.push_back(frame.locals[ins.a]);
                    break;

                case OpCode::STORE_LOCAL:
                    frame.locals[ins.a] = pop();
                    break;

                case OpCode::LOAD_GLOBAL:
                    stack_.push_back(globals_[ins.a]);
                    break;

                case OpCode::STORE_GLOBAL:
                    globals_[ins.a] = pop();
                    break;

                case OpCode::LOAD_SELF:
                    stack_.push_back(frame.self);
                    break;

                case OpCode::ADD: {
                    Value b = pop();
                    Value a = pop();
                    if (a.kind == ValueKind::Tekst || b.kind == ValueKind::Tekst) {
                        stack_.push_back(Value::tekst(a.toString() + b.toString()));
                    } else if (a.kind == ValueKind::Cijeli && b.kind == ValueKind::Cijeli) {
                        stack_.push_back(Value::cijeli(a.intVal + b.intVal));
                    } else {
                        stack_.push_back(Value::decimalni(a.asNumber() + b.asNumber()));
                    }
                    break;
                }

                case OpCode::SUB: {
                    Value b = pop();
                    Value a = pop();
                    if (a.kind == ValueKind::Cijeli && b.kind == ValueKind::Cijeli) {
                        stack_.push_back(Value::cijeli(a.intVal - b.intVal));
                    } else {
                        stack_.push_back(Value::decimalni(a.asNumber() - b.asNumber()));
                    }
                    break;
                }

                case OpCode::MUL: {
                    Value b = pop();
                    Value a = pop();
                    if (a.kind == ValueKind::Cijeli && b.kind == ValueKind::Cijeli) {
                        stack_.push_back(Value::cijeli(a.intVal * b.intVal));
                    } else {
                        stack_.push_back(Value::decimalni(a.asNumber() * b.asNumber()));
                    }
                    break;
                }

                case OpCode::DIV: {
                    Value b = pop();
                    Value a = pop();
                    if (a.kind == ValueKind::Cijeli && b.kind == ValueKind::Cijeli) {
                        if (b.intVal == 0) {
                            throw BosRuntimeException("Dijeljenje sa nulom nije dozvoljeno.", 101);
                        }
                        stack_.push_back(Value::cijeli(a.intVal / b.intVal));
                    } else {
                        double denom = b.asNumber();
                        if (denom == 0.0) {
                            throw BosRuntimeException("Dijeljenje sa nulom nije dozvoljeno.", 101);
                        }
                        stack_.push_back(Value::decimalni(a.asNumber() / denom));
                    }
                    break;
                }

                case OpCode::MOD: {
                    Value b = pop();
                    Value a = pop();
                    if (b.intVal == 0) {
                        throw BosRuntimeException("Dijeljenje sa nulom (ostatak '%') nije dozvoljeno.", 102);
                    }
                    stack_.push_back(Value::cijeli(a.intVal % b.intVal));
                    break;
                }

                case OpCode::NEG: {
                    Value a = pop();
                    if (a.kind == ValueKind::Cijeli) stack_.push_back(Value::cijeli(-a.intVal));
                    else stack_.push_back(Value::decimalni(-a.asNumber()));
                    break;
                }

                case OpCode::NOT: {
                    Value a = pop();
                    stack_.push_back(Value::logicki(!a.boolVal));
                    break;
                }

                case OpCode::EQ: {
                    Value b = pop();
                    Value a = pop();
                    stack_.push_back(Value::logicki(a.equals(b)));
                    break;
                }

                case OpCode::NEQ: {
                    Value b = pop();
                    Value a = pop();
                    stack_.push_back(Value::logicki(!a.equals(b)));
                    break;
                }

                case OpCode::LT: {
                    Value b = pop();
                    Value a = pop();
                    bool res = false;
                    if (a.kind == ValueKind::Tekst && b.kind == ValueKind::Tekst) res = a.strVal < b.strVal;
                    else if (a.kind == ValueKind::Znak && b.kind == ValueKind::Znak) res = a.charVal < b.charVal;
                    else res = a.asNumber() < b.asNumber();
                    stack_.push_back(Value::logicki(res));
                    break;
                }

                case OpCode::LTE: {
                    Value b = pop();
                    Value a = pop();
                    bool res = false;
                    if (a.kind == ValueKind::Tekst && b.kind == ValueKind::Tekst) res = a.strVal <= b.strVal;
                    else if (a.kind == ValueKind::Znak && b.kind == ValueKind::Znak) res = a.charVal <= b.charVal;
                    else res = a.asNumber() <= b.asNumber();
                    stack_.push_back(Value::logicki(res));
                    break;
                }

                case OpCode::GT: {
                    Value b = pop();
                    Value a = pop();
                    bool res = false;
                    if (a.kind == ValueKind::Tekst && b.kind == ValueKind::Tekst) res = a.strVal > b.strVal;
                    else if (a.kind == ValueKind::Znak && b.kind == ValueKind::Znak) res = a.charVal > b.charVal;
                    else res = a.asNumber() > b.asNumber();
                    stack_.push_back(Value::logicki(res));
                    break;
                }

                case OpCode::GTE: {
                    Value b = pop();
                    Value a = pop();
                    bool res = false;
                    if (a.kind == ValueKind::Tekst && b.kind == ValueKind::Tekst) res = a.strVal >= b.strVal;
                    else if (a.kind == ValueKind::Znak && b.kind == ValueKind::Znak) res = a.charVal >= b.charVal;
                    else res = a.asNumber() >= b.asNumber();
                    stack_.push_back(Value::logicki(res));
                    break;
                }

                case OpCode::JUMP:
                    frame.ip = static_cast<size_t>(ins.a);
                    break;

                case OpCode::JUMP_IF_FALSE: {
                    Value cond = pop();
                    if (!cond.boolVal) frame.ip = static_cast<size_t>(ins.a);
                    break;
                }

                case OpCode::JUMP_IF_TRUE: {
                    Value cond = pop();
                    if (cond.boolVal) frame.ip = static_cast<size_t>(ins.a);
                    break;
                }

                case OpCode::CALL_FUNC: {
                    int32_t fnIdx = ins.a;
                    int32_t argc = ins.b;
                    if (frames.size() >= options_.maxCallDepth) {
                        throw BosRuntimeException("Prekoračenje dubine poziva funkcija (prevelika rekurzija).", 201);
                    }
                    const FunctionIR& targetFn = module.functions[fnIdx];
                    CallFrame newFrame;
                    newFrame.funcIndex = fnIdx;
                    newFrame.ip = 0;
                    newFrame.locals.assign(static_cast<size_t>(targetFn.localCount), Value::prazno());
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        newFrame.locals[i] = pop();
                    }
                    frames.push_back(std::move(newFrame));
                    break;
                }

                case OpCode::CALL_METHOD: {
                    std::string methodName = fn.constants[ins.a].strVal;
                    int32_t argc = ins.b;
                    std::vector<Value> args(argc);
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        args[i] = pop();
                    }
                    Value receiver = pop();
                    if (receiver.kind != ValueKind::Objekt || !receiver.objVal) {
                        throw BosRuntimeException("Pokušaj poziva metode '" + methodName + "' nad neispravnim objektom.", 202);
                    }
                    int32_t mIdx = module.resolveClassMethod(receiver.objVal->typeName, methodName);
                    if (mIdx < 0) {
                        throw BosRuntimeException("Klasa '" + receiver.objVal->typeName + "' nema metodu '" + methodName + "'.", 203);
                    }
                    if (frames.size() >= options_.maxCallDepth) {
                        throw BosRuntimeException("Prekoračenje dubine poziva metoda (prevelika rekurzija).", 201);
                    }
                    const FunctionIR& targetFn = module.functions[mIdx];
                    CallFrame newFrame;
                    newFrame.funcIndex = mIdx;
                    newFrame.ip = 0;
                    newFrame.self = receiver;
                    newFrame.locals.assign(static_cast<size_t>(targetFn.localCount), Value::prazno());
                    for (int32_t i = 0; i < argc; ++i) {
                        newFrame.locals[i] = std::move(args[i]);
                    }
                    frames.push_back(std::move(newFrame));
                    break;
                }

                case OpCode::CALL_MODULE: {
                    std::string fullName = fn.constants[ins.a].strVal;
                    int32_t argc = ins.b;
                    std::vector<Value> args(argc);
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        args[i] = pop();
                    }
                    size_t dot = fullName.find('.');
                    std::string modName = fullName.substr(0, dot);
                    std::string funcName = fullName.substr(dot + 1);
                    stack_.push_back(stdlib_.callModuleFunction(modName, funcName, args));
                    break;
                }

                case OpCode::CALL_BUILTIN: {
                    std::string methodName = fn.constants[ins.a].strVal;
                    int32_t argc = ins.b;
                    std::vector<Value> args(argc);
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        args[i] = pop();
                    }
                    Value receiver = pop();
                    stack_.push_back(stdlib_.callBuiltinMethod(receiver, methodName, args));
                    break;
                }

                case OpCode::RETURN: {
                    Value retVal = pop();
                    bool returnSelf = frame.returnSelfOnExit;
                    Value selfObj = frame.self;
                    frames.pop_back();
                    if (returnSelf) {
                        stack_.push_back(selfObj);
                    } else {
                        stack_.push_back(std::move(retVal));
                    }
                    break;
                }

                case OpCode::NEW_STRUCT: {
                    const StructIR& st = module.structs[ins.a];
                    int32_t argc = ins.b;
                    std::vector<Value> args(argc);
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        args[i] = pop();
                    }
                    Value obj = Value::objekt(st.name, false);
                    for (size_t i = 0; i < st.fields.size(); ++i) {
                        if (static_cast<int32_t>(i) < argc) {
                            obj.objVal->fields[st.fields[i].name] = std::move(args[i]);
                        } else {
                            obj.objVal->fields[st.fields[i].name] = defaultValueForTag(st.fields[i].typeTag);
                        }
                    }
                    stack_.push_back(std::move(obj));
                    break;
                }

                case OpCode::NEW_CLASS: {
                    const ClassIR& cl = module.classes[ins.a];
                    int32_t argc = ins.b;
                    std::vector<Value> args(argc);
                    for (int32_t i = argc - 1; i >= 0; --i) {
                        args[i] = pop();
                    }
                    Value obj = Value::objekt(cl.name, true);
                    for (const auto& f : cl.fields) {
                        obj.objVal->fields[f.name] = defaultValueForTag(f.typeTag);
                    }
                    if (cl.constructorFuncIndex >= 0) {
                        const FunctionIR& ctorFn = module.functions[cl.constructorFuncIndex];
                        CallFrame ctorFrame;
                        ctorFrame.funcIndex = cl.constructorFuncIndex;
                        ctorFrame.ip = 0;
                        ctorFrame.self = obj;
                        ctorFrame.returnSelfOnExit = true;
                        ctorFrame.locals.assign(static_cast<size_t>(ctorFn.localCount), Value::prazno());
                        for (int32_t i = 0; i < argc; ++i) {
                            ctorFrame.locals[i] = std::move(args[i]);
                        }
                        frames.push_back(std::move(ctorFrame));
                    } else {
                        stack_.push_back(std::move(obj));
                    }
                    break;
                }

                case OpCode::GET_FIELD: {
                    std::string fieldName = fn.constants[ins.a].strVal;
                    Value obj = pop();
                    if (obj.kind == ValueKind::Greska && obj.errVal) {
                        if (fieldName == "poruka") stack_.push_back(Value::tekst(obj.errVal->message));
                        else if (fieldName == "kod") stack_.push_back(Value::cijeli(obj.errVal->code));
                        else throw BosRuntimeException("Greska nema polje '" + fieldName + "'.", 204);
                    } else if (obj.kind == ValueKind::Objekt && obj.objVal) {
                        auto it = obj.objVal->fields.find(fieldName);
                        if (it == obj.objVal->fields.end()) {
                            throw BosRuntimeException("Objekt tipa '" + obj.objVal->typeName + "' nema polje '" + fieldName + "'.", 204);
                        }
                        stack_.push_back(it->second);
                    } else {
                        throw BosRuntimeException("Pristup polju '" + fieldName + "' nije moguć na ovoj vrijednosti.", 204);
                    }
                    break;
                }

                case OpCode::SET_FIELD: {
                    std::string fieldName = fn.constants[ins.a].strVal;
                    Value val = pop();
                    Value obj = pop();
                    if (obj.kind != ValueKind::Objekt || !obj.objVal) {
                        throw BosRuntimeException("Postavljanje polja '" + fieldName + "' nije moguće na ovoj vrijednosti.", 204);
                    }
                    obj.objVal->fields[fieldName] = std::move(val);
                    break;
                }

                case OpCode::BUILD_NIZ: {
                    int32_t count = ins.a;
                    std::vector<Value> elems(count);
                    for (int32_t i = count - 1; i >= 0; --i) elems[i] = pop();
                    stack_.push_back(Value::niz(std::move(elems)));
                    break;
                }

                case OpCode::BUILD_LISTA: {
                    int32_t count = ins.a;
                    std::vector<Value> elems(count);
                    for (int32_t i = count - 1; i >= 0; --i) elems[i] = pop();
                    stack_.push_back(Value::lista(std::move(elems)));
                    break;
                }

                case OpCode::BUILD_RJECNIK: {
                    int32_t pairCount = ins.a;
                    std::vector<std::pair<Value, Value>> pairs(pairCount);
                    for (int32_t i = pairCount - 1; i >= 0; --i) {
                        Value v = pop();
                        Value k = pop();
                        pairs[i] = {std::move(k), std::move(v)};
                    }
                    Value dict = Value::rjecnik();
                    for (auto& kv : pairs) {
                        dict.dictVal->set(kv.first.toString(), std::move(kv.second));
                    }
                    stack_.push_back(std::move(dict));
                    break;
                }

                case OpCode::GET_INDEX: {
                    Value idx = pop();
                    Value obj = pop();
                    if ((obj.kind == ValueKind::Niz || obj.kind == ValueKind::Lista) && obj.listVal) {
                        int64_t i = idx.intVal;
                        if (i < 0 || static_cast<size_t>(i) >= obj.listVal->elements.size()) {
                            throw BosRuntimeException("Indeks " + std::to_string(i) + " je izvan opsega kolekcije (veličina "
                                + std::to_string(obj.listVal->elements.size()) + ").", 103);
                        }
                        stack_.push_back(obj.listVal->elements[i]);
                    } else if (obj.kind == ValueKind::Tekst) {
                        stack_.push_back(Value::znak(StandardLibrary::utf8CharAt(obj.strVal, idx.intVal)));
                    } else if (obj.kind == ValueKind::Rjecnik && obj.dictVal) {
                        std::string key = idx.toString();
                        Value* found = obj.dictVal->find(key);
                        if (!found) {
                            throw BosRuntimeException("Ključ '" + key + "' nije pronađen u rječniku.", 103);
                        }
                        stack_.push_back(*found);
                    } else {
                        throw BosRuntimeException("Indeksiranje nije podržano za ovaj tip vrijednosti.", 103);
                    }
                    break;
                }

                case OpCode::SET_INDEX: {
                    Value val = pop();
                    Value idx = pop();
                    Value obj = pop();
                    if ((obj.kind == ValueKind::Niz || obj.kind == ValueKind::Lista) && obj.listVal) {
                        int64_t i = idx.intVal;
                        if (i < 0 || static_cast<size_t>(i) >= obj.listVal->elements.size()) {
                            throw BosRuntimeException("Indeks " + std::to_string(i) + " je izvan opsega kolekcije.", 103);
                        }
                        obj.listVal->elements[i] = std::move(val);
                    } else if (obj.kind == ValueKind::Rjecnik && obj.dictVal) {
                        obj.dictVal->set(idx.toString(), std::move(val));
                    } else {
                        throw BosRuntimeException("Dodjela preko indeksa nije podržana za ovaj tip.", 103);
                    }
                    break;
                }

                case OpCode::ITER_LEN: {
                    Value it = pop();
                    if ((it.kind == ValueKind::Niz || it.kind == ValueKind::Lista) && it.listVal) {
                        stack_.push_back(Value::cijeli(static_cast<int64_t>(it.listVal->elements.size())));
                    } else if (it.kind == ValueKind::Tekst) {
                        stack_.push_back(Value::cijeli(StandardLibrary::utf8Length(it.strVal)));
                    } else {
                        throw BosRuntimeException("Vrijednost se ne može iterirati.", 103);
                    }
                    break;
                }

                case OpCode::ITER_GET: {
                    Value idx = pop();
                    Value it = pop();
                    if ((it.kind == ValueKind::Niz || it.kind == ValueKind::Lista) && it.listVal) {
                        stack_.push_back(it.listVal->elements[idx.intVal]);
                    } else if (it.kind == ValueKind::Tekst) {
                        stack_.push_back(Value::znak(StandardLibrary::utf8CharAt(it.strVal, idx.intVal)));
                    } else {
                        throw BosRuntimeException("Vrijednost se ne može iterirati.", 103);
                    }
                    break;
                }

                case OpCode::PRINT: {
                    Value val = pop();
                    if (options_.output) {
                        (*options_.output) << val.toString() << "\n";
                    }
                    break;
                }

                case OpCode::INPUT: {
                    std::string line;
                    if (options_.input && std::getline(*options_.input, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                    }
                    switch (ins.a) {
                        case 1: { // cijeli
                            try {
                                size_t consumed = 0;
                                int64_t v = std::stoll(line, &consumed);
                                stack_.push_back(Value::cijeli(v));
                            } catch (...) {
                                throw BosRuntimeException("Neispravan unos za tip 'cijeli': '" + line + "'.", 111);
                            }
                            break;
                        }
                        case 2: { // decimalni
                            try {
                                double v = std::stod(line);
                                stack_.push_back(Value::decimalni(v));
                            } catch (...) {
                                throw BosRuntimeException("Neispravan unos za tip 'decimalni': '" + line + "'.", 111);
                            }
                            break;
                        }
                        case 3: { // logički
                            if (line == "tačno" || line == "tacno" || line == "true" || line == "1") {
                                stack_.push_back(Value::logicki(true));
                            } else if (line == "netačno" || line == "netacno" || line == "false" || line == "0") {
                                stack_.push_back(Value::logicki(false));
                            } else {
                                throw BosRuntimeException("Neispravan unos za tip 'logički': '" + line + "'.", 111);
                            }
                            break;
                        }
                        case 4: { // znak
                            char32_t cp = 0;
                            size_t len = 1;
                            if (!line.empty()) Lexer::decodeUtf8(line, 0, cp, len);
                            stack_.push_back(Value::znak(cp));
                            break;
                        }
                        default:
                            stack_.push_back(Value::tekst(line));
                            break;
                    }
                    break;
                }

                case OpCode::IMPORT_MODULE: {
                    std::string modName = fn.constants[ins.a].strVal;
                    globals_[ins.b] = Value::modul(modName);
                    break;
                }

                case OpCode::GET_MODULE_CONST: {
                    std::string fullName = fn.constants[ins.a].strVal;
                    size_t dot = fullName.find('.');
                    std::string modName = fullName.substr(0, dot);
                    std::string constName = fullName.substr(dot + 1);
                    stack_.push_back(stdlib_.getModuleConstant(modName, constName));
                    break;
                }

                case OpCode::PUSH_TRY:
                    tryHandlers.push_back(TryHandler{frames.size(), static_cast<size_t>(ins.a), stack_.size()});
                    break;

                case OpCode::POP_TRY:
                    if (!tryHandlers.empty()) tryHandlers.pop_back();
                    break;

                case OpCode::THROW: {
                    Value thrown = pop();
                    if (thrown.kind == ValueKind::Greska && thrown.errVal) {
                        throw BosRuntimeException(thrown.errVal->message, thrown.errVal->code);
                    }
                    throw BosRuntimeException(thrown.toString(), 1);
                }

                case OpCode::HALT:
                    frames.clear();
                    break;
            }
        } catch (const BosRuntimeException& ex) {
            Value errVal = Value::greska(ex.what(), ex.code);
            if (!dispatchException(errVal, frames, tryHandlers)) {
                result.ok = false;
                result.stepsExecuted = steps;
                result.errorLine = curLine;
                result.runtimeError = ex.what();
                diagnostics.error(DiagnosticStage::Runtime,
                                   SourceLocation{module.filename, curLine, 1},
                                   ex.what(), "R" + std::to_string(ex.code));
                return result;
            }
        }
    }

    result.ok = true;
    result.stepsExecuted = steps;
    return result;
}

} // namespace bos
