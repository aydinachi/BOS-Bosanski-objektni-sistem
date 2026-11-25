#include "ir/ir.hpp"
#include "lexer/lexer.hpp"

#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace bos {

std::string Constant::toString() const {
    switch (kind) {
        case ConstKind::Prazno: return "prazno";
        case ConstKind::Cijeli: return std::to_string(intVal);
        case ConstKind::Decimalni: {
            std::ostringstream oss; oss << floatVal; return oss.str();
        }
        case ConstKind::Logicki: return boolVal ? "tačno" : "netačno";
        case ConstKind::Znak: return "'" + Lexer::encodeUtf8(charVal) + "'";
        case ConstKind::Tekst: return "\"" + strVal + "\"";
    }
    return "prazno";
}

const char* opCodeName(OpCode op) {
    switch (op) {
        case OpCode::PUSH_CONST: return "PUSH_CONST";
        case OpCode::PUSH_PRAZNO: return "PUSH_PRAZNO";
        case OpCode::POP: return "POP";
        case OpCode::DUP: return "DUP";
        case OpCode::LOAD_LOCAL: return "LOAD_LOCAL";
        case OpCode::STORE_LOCAL: return "STORE_LOCAL";
        case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL";
        case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
        case OpCode::LOAD_SELF: return "LOAD_SELF";
        case OpCode::ADD: return "ADD";
        case OpCode::SUB: return "SUB";
        case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV";
        case OpCode::MOD: return "MOD";
        case OpCode::NEG: return "NEG";
        case OpCode::NOT: return "NOT";
        case OpCode::EQ: return "EQ";
        case OpCode::NEQ: return "NEQ";
        case OpCode::LT: return "LT";
        case OpCode::LTE: return "LTE";
        case OpCode::GT: return "GT";
        case OpCode::GTE: return "GTE";
        case OpCode::JUMP: return "JUMP";
        case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
        case OpCode::CALL_FUNC: return "CALL_FUNC";
        case OpCode::CALL_METHOD: return "CALL_METHOD";
        case OpCode::CALL_MODULE: return "CALL_MODULE";
        case OpCode::CALL_BUILTIN: return "CALL_BUILTIN";
        case OpCode::RETURN: return "RETURN";
        case OpCode::NEW_STRUCT: return "NEW_STRUCT";
        case OpCode::NEW_CLASS: return "NEW_CLASS";
        case OpCode::GET_FIELD: return "GET_FIELD";
        case OpCode::SET_FIELD: return "SET_FIELD";
        case OpCode::BUILD_NIZ: return "BUILD_NIZ";
        case OpCode::BUILD_LISTA: return "BUILD_LISTA";
        case OpCode::BUILD_RJECNIK: return "BUILD_RJECNIK";
        case OpCode::GET_INDEX: return "GET_INDEX";
        case OpCode::SET_INDEX: return "SET_INDEX";
        case OpCode::ITER_LEN: return "ITER_LEN";
        case OpCode::ITER_GET: return "ITER_GET";
        case OpCode::PRINT: return "PRINT";
        case OpCode::INPUT: return "INPUT";
        case OpCode::IMPORT_MODULE: return "IMPORT_MODULE";
        case OpCode::GET_MODULE_CONST: return "GET_MODULE_CONST";
        case OpCode::PUSH_TRY: return "PUSH_TRY";
        case OpCode::POP_TRY: return "POP_TRY";
        case OpCode::THROW: return "THROW";
        case OpCode::HALT: return "HALT";
    }
    return "UNKNOWN";
}

int32_t FunctionIR::addConstant(const Constant& c) {
    for (size_t i = 0; i < constants.size(); ++i) {
        const auto& ex = constants[i];
        if (ex.kind != c.kind) continue;
        bool same = false;
        switch (c.kind) {
            case ConstKind::Prazno: same = true; break;
            case ConstKind::Cijeli: same = (ex.intVal == c.intVal); break;
            case ConstKind::Decimalni: same = (ex.floatVal == c.floatVal); break;
            case ConstKind::Logicki: same = (ex.boolVal == c.boolVal); break;
            case ConstKind::Znak: same = (ex.charVal == c.charVal); break;
            case ConstKind::Tekst: same = (ex.strVal == c.strVal); break;
        }
        if (same) return static_cast<int32_t>(i);
    }
    constants.push_back(c);
    return static_cast<int32_t>(constants.size() - 1);
}

int32_t FunctionIR::emit(OpCode op, int32_t a, int32_t b, int32_t line) {
    code.push_back(Instruction{op, a, b, line});
    return static_cast<int32_t>(code.size() - 1);
}

int32_t BytecodeModule::findFunction(const std::string& name) const {
    for (size_t i = 0; i < functions.size(); ++i) {
        if (functions[i].ownerClass.empty() && functions[i].name == name) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t BytecodeModule::findStruct(const std::string& name) const {
    for (size_t i = 0; i < structs.size(); ++i) {
        if (structs[i].name == name) return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t BytecodeModule::findClass(const std::string& name) const {
    for (size_t i = 0; i < classes.size(); ++i) {
        if (classes[i].name == name) return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t BytecodeModule::resolveClassMethod(const std::string& className, const std::string& methodName) const {
    std::unordered_set<std::string> visited;
    std::string cur = className;
    while (!cur.empty() && visited.insert(cur).second) {
        int32_t idx = findClass(cur);
        if (idx < 0) return -1;
        const auto& cl = classes[idx];
        auto it = cl.methods.find(methodName);
        if (it != cl.methods.end()) return it->second;
        cur = cl.baseClass;
    }
    return -1;
}

std::string BytecodeModule::disassemble() const {
    std::ostringstream oss;
    oss << "; BOS Bytecode IR Module: " << filename << "\n";
    if (!globalNames.empty()) {
        oss << "; Globals (" << globalNames.size() << "): ";
        for (size_t i = 0; i < globalNames.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "[" << i << "]=" << globalNames[i];
        }
        oss << "\n";
    }
    for (const auto& st : structs) {
        oss << "; Zapis " << st.name << " { ";
        for (size_t i = 0; i < st.fields.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << st.fields[i].name;
        }
        oss << " }\n";
    }
    for (const auto& cl : classes) {
        oss << "; Klasa " << cl.name;
        if (!cl.baseClass.empty()) oss << " nasljeduje " << cl.baseClass;
        oss << " (metode: " << cl.methods.size() << ")\n";
    }

    for (size_t fi = 0; fi < functions.size(); ++fi) {
        const auto& fn = functions[fi];
        oss << "\nfunc #" << fi << " " << (fn.ownerClass.empty() ? "" : (fn.ownerClass + "::"))
            << fn.name << " (params=" << fn.paramCount << ", locals=" << fn.localCount << "):\n";
        for (size_t ip = 0; ip < fn.code.size(); ++ip) {
            const auto& ins = fn.code[ip];
            oss << "  " << std::setw(4) << std::setfill('0') << ip << "  "
                << std::left << std::setw(18) << std::setfill(' ') << opCodeName(ins.op);
            switch (ins.op) {
                case OpCode::PUSH_CONST:
                case OpCode::GET_FIELD:
                case OpCode::SET_FIELD:
                case OpCode::GET_MODULE_CONST:
                    oss << ins.a;
                    if (ins.a >= 0 && static_cast<size_t>(ins.a) < fn.constants.size()) {
                        oss << "  ; " << fn.constants[ins.a].toString();
                    }
                    break;
                case OpCode::CALL_MODULE:
                case OpCode::CALL_METHOD:
                case OpCode::CALL_BUILTIN:
                    oss << ins.a << ", argc=" << ins.b;
                    if (ins.a >= 0 && static_cast<size_t>(ins.a) < fn.constants.size()) {
                        oss << "  ; " << fn.constants[ins.a].toString();
                    }
                    break;
                case OpCode::CALL_FUNC:
                    oss << "fn#" << ins.a << ", argc=" << ins.b;
                    if (ins.a >= 0 && static_cast<size_t>(ins.a) < functions.size()) {
                        oss << "  ; " << functions[ins.a].name;
                    }
                    break;
                case OpCode::LOAD_GLOBAL:
                case OpCode::STORE_GLOBAL:
                    oss << ins.a;
                    if (ins.a >= 0 && static_cast<size_t>(ins.a) < globalNames.size()) {
                        oss << "  ; " << globalNames[ins.a];
                    }
                    break;
                case OpCode::LOAD_LOCAL:
                case OpCode::STORE_LOCAL:
                case OpCode::JUMP:
                case OpCode::JUMP_IF_FALSE:
                case OpCode::JUMP_IF_TRUE:
                case OpCode::PUSH_TRY:
                case OpCode::BUILD_NIZ:
                case OpCode::BUILD_LISTA:
                case OpCode::BUILD_RJECNIK:
                case OpCode::INPUT:
                    oss << ins.a;
                    break;
                case OpCode::NEW_STRUCT:
                case OpCode::NEW_CLASS:
                case OpCode::IMPORT_MODULE:
                    oss << ins.a << ", " << ins.b;
                    break;
                default:
                    break;
            }
            oss << "\n";
        }
    }
    return oss.str();
}

} // namespace bos
