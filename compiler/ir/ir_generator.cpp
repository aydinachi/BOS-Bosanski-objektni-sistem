#include "ir/ir_generator.hpp"

#include <algorithm>
#include <unordered_set>

namespace bos {

IRGenerator::IRGenerator(const SymbolTable& symbols, DiagnosticEngine& diagnostics)
    : symbols_(symbols), diagnostics_(diagnostics) {}

int32_t IRGenerator::getOrAddGlobal(const std::string& name) {
    auto it = globalMap_.find(name);
    if (it != globalMap_.end()) return it->second;
    int32_t idx = static_cast<int32_t>(module_.globalNames.size());
    module_.globalNames.push_back(name);
    globalMap_[name] = idx;
    return idx;
}

int32_t IRGenerator::lookupGlobal(const std::string& name) const {
    auto it = globalMap_.find(name);
    return it == globalMap_.end() ? -1 : it->second;
}

void IRGenerator::pushLocalScope() {
    localScopes_.emplace_back();
}

void IRGenerator::popLocalScope() {
    if (!localScopes_.empty()) {
        localScopes_.pop_back();
    }
}

int32_t IRGenerator::declareLocal(const std::string& name) {
    int32_t slot = nextLocalSlot_++;
    if (nextLocalSlot_ > maxLocalSlot_) maxLocalSlot_ = nextLocalSlot_;
    if (!localScopes_.empty()) {
        localScopes_.back()[name] = slot;
    }
    return slot;
}

int32_t IRGenerator::lookupLocal(const std::string& name) const {
    for (auto it = localScopes_.rbegin(); it != localScopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    return -1;
}

uint8_t IRGenerator::typeToDefaultTag(const TypePtr& type) const {
    if (!type) return 0;
    switch (type->kind) {
        case TypeKind::Cijeli: return 1;
        case TypeKind::Decimalni: return 2;
        case TypeKind::Tekst: return 3;
        case TypeKind::Znak: return 4;
        case TypeKind::Logicki: return 5;
        case TypeKind::Niz: return 6;
        case TypeKind::Lista: return 7;
        case TypeKind::Rjecnik: return 8;
        default: return 0;
    }
}

int32_t IRGenerator::typeToInputTag(const TypePtr& type) const {
    if (!type) return 0;
    switch (type->kind) {
        case TypeKind::Tekst: return 0;
        case TypeKind::Cijeli: return 1;
        case TypeKind::Decimalni: return 2;
        case TypeKind::Logicki: return 3;
        case TypeKind::Znak: return 4;
        default: return 0;
    }
}

void IRGenerator::emitDefaultValue(const TypePtr& type, int32_t line) {
    if (!type) {
        curFn().emit(OpCode::PUSH_PRAZNO, 0, 0, line);
        return;
    }
    switch (type->kind) {
        case TypeKind::Cijeli: {
            int32_t c = curFn().addConstant(Constant::cijeli(0));
            curFn().emit(OpCode::PUSH_CONST, c, 0, line);
            break;
        }
        case TypeKind::Decimalni: {
            int32_t c = curFn().addConstant(Constant::decimalni(0.0));
            curFn().emit(OpCode::PUSH_CONST, c, 0, line);
            break;
        }
        case TypeKind::Tekst: {
            int32_t c = curFn().addConstant(Constant::tekst(""));
            curFn().emit(OpCode::PUSH_CONST, c, 0, line);
            break;
        }
        case TypeKind::Znak: {
            int32_t c = curFn().addConstant(Constant::znak(0));
            curFn().emit(OpCode::PUSH_CONST, c, 0, line);
            break;
        }
        case TypeKind::Logicki: {
            int32_t c = curFn().addConstant(Constant::logicki(false));
            curFn().emit(OpCode::PUSH_CONST, c, 0, line);
            break;
        }
        case TypeKind::Niz:
            curFn().emit(OpCode::BUILD_NIZ, 0, 0, line);
            break;
        case TypeKind::Lista:
            curFn().emit(OpCode::BUILD_LISTA, 0, 0, line);
            break;
        case TypeKind::Rjecnik:
            curFn().emit(OpCode::BUILD_RJECNIK, 0, 0, line);
            break;
        case TypeKind::Zapis: {
            int32_t idx = module_.findStruct(type->name);
            curFn().emit(OpCode::NEW_STRUCT, idx, 0, line);
            break;
        }
        case TypeKind::Klasa: {
            int32_t idx = module_.findClass(type->name);
            curFn().emit(OpCode::NEW_CLASS, idx, 0, line);
            break;
        }
        default:
            curFn().emit(OpCode::PUSH_PRAZNO, 0, 0, line);
            break;
    }
}

void IRGenerator::registerMetadata(const Program& program) {
    // Function 0 is always __main__
    FunctionIR mainFn;
    mainFn.name = "__main__";
    module_.functions.push_back(std::move(mainFn));

    // Register structs, classes, and reserve function indices
    for (const auto& stmt : program.statements) {
        if (auto* st = dynamic_cast<const StructDeclStmt*>(stmt.get())) {
            StructIR sir;
            sir.name = st->name;
            for (const auto& f : st->fields) {
                sir.fields.push_back(FieldDefaultIR{f.name, typeToDefaultTag(f.resolvedType)});
            }
            module_.structs.push_back(std::move(sir));
        } else if (auto* cl = dynamic_cast<const ClassDeclStmt*>(stmt.get())) {
            ClassIR cir;
            cir.name = cl->name;
            cir.baseClass = cl->baseClass;
            for (const auto& f : cl->fields) {
                cir.fields.push_back(FieldDefaultIR{f.name, typeToDefaultTag(f.resolvedType)});
            }
            for (const auto& m : cl->methods) {
                FunctionIR fir;
                fir.name = m->name;
                fir.ownerClass = cl->name;
                fir.paramCount = static_cast<int32_t>(m->params.size());
                fir.isConstructor = m->isConstructor;
                int32_t idx = static_cast<int32_t>(module_.functions.size());
                module_.functions.push_back(std::move(fir));
                funcIndexMap_[cl->name + "::" + m->name] = idx;
                if (m->isConstructor) {
                    cir.constructorFuncIndex = idx;
                } else {
                    cir.methods[m->name] = idx;
                }
            }
            module_.classes.push_back(std::move(cir));
        } else if (auto* fn = dynamic_cast<const FuncDeclStmt*>(stmt.get())) {
            FunctionIR fir;
            fir.name = fn->name;
            fir.paramCount = static_cast<int32_t>(fn->params.size());
            int32_t idx = static_cast<int32_t>(module_.functions.size());
            module_.functions.push_back(std::move(fir));
            funcIndexMap_[fn->name] = idx;
        }
    }

    // Flatten inherited fields onto derived classes (base fields first)
    for (auto& cl : module_.classes) {
        if (cl.baseClass.empty()) continue;
        std::vector<FieldDefaultIR> allFields;
        std::vector<std::string> chain;
        std::unordered_set<std::string> visited;
        std::string cur = cl.name;
        while (!cur.empty() && visited.insert(cur).second) {
            chain.push_back(cur);
            int32_t ci = module_.findClass(cur);
            cur = (ci >= 0) ? module_.classes[ci].baseClass : "";
        }
        std::reverse(chain.begin(), chain.end());
        std::unordered_set<std::string> seen;
        for (const auto& cname : chain) {
            int32_t ci = module_.findClass(cname);
            if (ci < 0) continue;
            for (const auto& f : module_.classes[ci].fields) {
                if (seen.insert(f.name).second) {
                    allFields.push_back(f);
                }
            }
        }
        cl.fields = std::move(allFields);
    }
}

void IRGenerator::compileFunction(const FuncDeclStmt& fn, int32_t funcIndex, const std::string& ownerClass) {
    int32_t prevFunc = curFuncIdx_;
    bool prevTop = inTopLevel_;
    std::string prevClass = curClass_;
    auto prevScopes = std::move(localScopes_);
    int32_t prevNext = nextLocalSlot_;
    int32_t prevMax = maxLocalSlot_;

    curFuncIdx_ = funcIndex;
    inTopLevel_ = false;
    curClass_ = ownerClass;
    localScopes_.clear();
    nextLocalSlot_ = 0;
    maxLocalSlot_ = 0;

    pushLocalScope();
    for (const auto& p : fn.params) {
        declareLocal(p.name);
    }

    for (const auto& s : fn.body) {
        emitStmt(*s);
    }

    // Implicit return prazno at end of function
    curFn().emit(OpCode::PUSH_PRAZNO, 0, 0, fn.location.line);
    curFn().emit(OpCode::RETURN, 0, 0, fn.location.line);
    curFn().localCount = maxLocalSlot_;

    popLocalScope();

    curFuncIdx_ = prevFunc;
    inTopLevel_ = prevTop;
    curClass_ = prevClass;
    localScopes_ = std::move(prevScopes);
    nextLocalSlot_ = prevNext;
    maxLocalSlot_ = prevMax;
}

BytecodeModule IRGenerator::generate(const Program& program) {
    module_ = BytecodeModule{};
    module_.filename = program.filename;
    globalMap_.clear();
    funcIndexMap_.clear();
    localScopes_.clear();
    loopStack_.clear();

    registerMetadata(program);

    // Compile all functions and class methods
    for (const auto& stmt : program.statements) {
        if (auto* fn = dynamic_cast<const FuncDeclStmt*>(stmt.get())) {
            int32_t idx = funcIndexMap_[fn->name];
            compileFunction(*fn, idx, "");
        } else if (auto* cl = dynamic_cast<const ClassDeclStmt*>(stmt.get())) {
            for (const auto& m : cl->methods) {
                int32_t idx = funcIndexMap_[cl->name + "::" + m->name];
                compileFunction(*m, idx, cl->name);
            }
        }
    }

    // Compile top-level statements into __main__ (index 0)
    curFuncIdx_ = 0;
    inTopLevel_ = true;
    curClass_.clear();
    nextLocalSlot_ = 0;
    maxLocalSlot_ = 0;
    pushLocalScope();

    for (const auto& stmt : program.statements) {
        if (dynamic_cast<const FuncDeclStmt*>(stmt.get()) ||
            dynamic_cast<const StructDeclStmt*>(stmt.get()) ||
            dynamic_cast<const ClassDeclStmt*>(stmt.get())) {
            continue;
        }
        emitStmt(*stmt);
    }

    curFn().emit(OpCode::HALT, 0, 0, 0);
    curFn().localCount = maxLocalSlot_;
    popLocalScope();

    return std::move(module_);
}

void IRGenerator::emitCompoundOp(AssignOp op, int32_t line) {
    switch (op) {
        case AssignOp::AddAssign: curFn().emit(OpCode::ADD, 0, 0, line); break;
        case AssignOp::SubAssign: curFn().emit(OpCode::SUB, 0, 0, line); break;
        case AssignOp::MulAssign: curFn().emit(OpCode::MUL, 0, 0, line); break;
        case AssignOp::DivAssign: curFn().emit(OpCode::DIV, 0, 0, line); break;
        default: break;
    }
}

void IRGenerator::emitStmt(const Stmt& stmt) {
    int32_t line = stmt.location.line;

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        if (v->initializer) {
            emitExpr(*v->initializer);
        } else {
            emitDefaultValue(v->resolvedType, line);
        }

        // Top-level outer scope variables go to globals; nested block or function vars go to locals
        if (inTopLevel_ && localScopes_.size() == 1) {
            int32_t g = getOrAddGlobal(v->name);
            curFn().emit(OpCode::STORE_GLOBAL, g, 0, line);
        } else {
            int32_t slot = declareLocal(v->name);
            curFn().emit(OpCode::STORE_LOCAL, slot, 0, line);
        }
        return;
    }

    if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) {
        if (auto* id = dynamic_cast<const IdentifierExpr*>(a->target.get())) {
            if (id->resolvedAsSelfField) {
                int32_t fc = curFn().addConstant(Constant::tekst(id->name));
                curFn().emit(OpCode::LOAD_SELF, 0, 0, line);
                if (a->op != AssignOp::Assign) {
                    curFn().emit(OpCode::DUP, 0, 0, line);
                    curFn().emit(OpCode::GET_FIELD, fc, 0, line);
                    emitExpr(*a->value);
                    emitCompoundOp(a->op, line);
                } else {
                    emitExpr(*a->value);
                }
                curFn().emit(OpCode::SET_FIELD, fc, 0, line);
                return;
            }
            int32_t local = lookupLocal(id->name);
            if (local >= 0) {
                if (a->op != AssignOp::Assign) {
                    curFn().emit(OpCode::LOAD_LOCAL, local, 0, line);
                    emitExpr(*a->value);
                    emitCompoundOp(a->op, line);
                } else {
                    emitExpr(*a->value);
                }
                curFn().emit(OpCode::STORE_LOCAL, local, 0, line);
            } else {
                int32_t g = getOrAddGlobal(id->name);
                if (a->op != AssignOp::Assign) {
                    curFn().emit(OpCode::LOAD_GLOBAL, g, 0, line);
                    emitExpr(*a->value);
                    emitCompoundOp(a->op, line);
                } else {
                    emitExpr(*a->value);
                }
                curFn().emit(OpCode::STORE_GLOBAL, g, 0, line);
            }
            return;
        }
        if (auto* mem = dynamic_cast<const MemberAccessExpr*>(a->target.get())) {
            int32_t fc = curFn().addConstant(Constant::tekst(mem->member));
            emitExpr(*mem->object);
            if (a->op != AssignOp::Assign) {
                curFn().emit(OpCode::DUP, 0, 0, line);
                curFn().emit(OpCode::GET_FIELD, fc, 0, line);
                emitExpr(*a->value);
                emitCompoundOp(a->op, line);
            } else {
                emitExpr(*a->value);
            }
            curFn().emit(OpCode::SET_FIELD, fc, 0, line);
            return;
        }
        if (auto* idx = dynamic_cast<const IndexExpr*>(a->target.get())) {
            emitExpr(*idx->object);
            emitExpr(*idx->index);
            if (a->op != AssignOp::Assign) {
                // Save obj and idx into temps so we don't evaluate side effects twice
                int32_t tObj = declareLocal("$t_obj_" + std::to_string(tempCounter_++));
                int32_t tIdx = declareLocal("$t_idx_" + std::to_string(tempCounter_++));
                curFn().emit(OpCode::STORE_LOCAL, tIdx, 0, line);
                curFn().emit(OpCode::STORE_LOCAL, tObj, 0, line);
                curFn().emit(OpCode::LOAD_LOCAL, tObj, 0, line);
                curFn().emit(OpCode::LOAD_LOCAL, tIdx, 0, line);
                curFn().emit(OpCode::LOAD_LOCAL, tObj, 0, line);
                curFn().emit(OpCode::LOAD_LOCAL, tIdx, 0, line);
                curFn().emit(OpCode::GET_INDEX, 0, 0, line);
                emitExpr(*a->value);
                emitCompoundOp(a->op, line);
            } else {
                emitExpr(*a->value);
            }
            curFn().emit(OpCode::SET_INDEX, 0, 0, line);
            return;
        }
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) {
        emitExpr(*e->expr);
        curFn().emit(OpCode::POP, 0, 0, line);
        return;
    }

    if (auto* p = dynamic_cast<const PrintStmt*>(&stmt)) {
        emitExpr(*p->expr);
        curFn().emit(OpCode::PRINT, 0, 0, line);
        return;
    }

    if (auto* in = dynamic_cast<const InputStmt*>(&stmt)) {
        int32_t tag = typeToInputTag(in->targetType);
        if (in->resolvedAsSelfField) {
            int32_t fc = curFn().addConstant(Constant::tekst(in->targetName));
            curFn().emit(OpCode::LOAD_SELF, 0, 0, line);
            curFn().emit(OpCode::INPUT, tag, 0, line);
            curFn().emit(OpCode::SET_FIELD, fc, 0, line);
            return;
        }
        curFn().emit(OpCode::INPUT, tag, 0, line);
        int32_t local = lookupLocal(in->targetName);
        if (local >= 0) {
            curFn().emit(OpCode::STORE_LOCAL, local, 0, line);
        } else {
            int32_t g = getOrAddGlobal(in->targetName);
            curFn().emit(OpCode::STORE_GLOBAL, g, 0, line);
        }
        return;
    }

    if (auto* iff = dynamic_cast<const IfStmt*>(&stmt)) {
        std::vector<int32_t> endJumps;

        emitExpr(*iff->condition);
        int32_t jumpIfFalse = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, line);

        pushLocalScope();
        for (const auto& s : iff->thenBranch) emitStmt(*s);
        popLocalScope();

        endJumps.push_back(curFn().emit(OpCode::JUMP, 0, 0, line));
        curFn().code[jumpIfFalse].a = static_cast<int32_t>(curFn().code.size());

        for (const auto& ei : iff->elseIfBranches) {
            emitExpr(*ei.condition);
            int32_t nextFalse = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, ei.location.line);
            pushLocalScope();
            for (const auto& s : ei.body) emitStmt(*s);
            popLocalScope();
            endJumps.push_back(curFn().emit(OpCode::JUMP, 0, 0, ei.location.line));
            curFn().code[nextFalse].a = static_cast<int32_t>(curFn().code.size());
        }

        if (!iff->elseBranch.empty()) {
            pushLocalScope();
            for (const auto& s : iff->elseBranch) emitStmt(*s);
            popLocalScope();
        }

        int32_t endIp = static_cast<int32_t>(curFn().code.size());
        for (int32_t j : endJumps) {
            curFn().code[j].a = endIp;
        }
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
        int32_t loopStart = static_cast<int32_t>(curFn().code.size());
        emitExpr(*w->condition);
        int32_t exitJump = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, line);

        loopStack_.push_back(LoopContext{});
        pushLocalScope();
        for (const auto& s : w->body) emitStmt(*s);
        popLocalScope();
        LoopContext ctx = std::move(loopStack_.back());
        loopStack_.pop_back();

        for (int32_t cj : ctx.continueJumps) {
            curFn().code[cj].a = loopStart;
        }
        curFn().emit(OpCode::JUMP, loopStart, 0, line);

        int32_t loopEnd = static_cast<int32_t>(curFn().code.size());
        curFn().code[exitJump].a = loopEnd;
        for (int32_t bj : ctx.breakJumps) {
            curFn().code[bj].a = loopEnd;
        }
        return;
    }

    if (auto* fr = dynamic_cast<const ForRangeStmt*>(&stmt)) {
        pushLocalScope();
        int32_t varSlot = declareLocal(fr->varName);
        int32_t endSlot = declareLocal("$end_" + std::to_string(tempCounter_++));

        emitExpr(*fr->start);
        curFn().emit(OpCode::STORE_LOCAL, varSlot, 0, line);
        emitExpr(*fr->end);
        curFn().emit(OpCode::STORE_LOCAL, endSlot, 0, line);

        int32_t condIp = static_cast<int32_t>(curFn().code.size());
        curFn().emit(OpCode::LOAD_LOCAL, varSlot, 0, line);
        curFn().emit(OpCode::LOAD_LOCAL, endSlot, 0, line);
        curFn().emit(OpCode::LTE, 0, 0, line);
        int32_t exitJump = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, line);

        loopStack_.push_back(LoopContext{});
        pushLocalScope();
        for (const auto& s : fr->body) emitStmt(*s);
        popLocalScope();
        LoopContext ctx = std::move(loopStack_.back());
        loopStack_.pop_back();

        int32_t stepIp = static_cast<int32_t>(curFn().code.size());
        for (int32_t cj : ctx.continueJumps) {
            curFn().code[cj].a = stepIp;
        }
        int32_t oneConst = curFn().addConstant(Constant::cijeli(1));
        curFn().emit(OpCode::LOAD_LOCAL, varSlot, 0, line);
        curFn().emit(OpCode::PUSH_CONST, oneConst, 0, line);
        curFn().emit(OpCode::ADD, 0, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, varSlot, 0, line);
        curFn().emit(OpCode::JUMP, condIp, 0, line);

        int32_t loopEnd = static_cast<int32_t>(curFn().code.size());
        curFn().code[exitJump].a = loopEnd;
        for (int32_t bj : ctx.breakJumps) {
            curFn().code[bj].a = loopEnd;
        }
        popLocalScope();
        return;
    }

    if (auto* fe = dynamic_cast<const ForEachStmt*>(&stmt)) {
        pushLocalScope();
        int32_t iterSlot = declareLocal("$iter_" + std::to_string(tempCounter_++));
        int32_t idxSlot = declareLocal("$idx_" + std::to_string(tempCounter_++));
        int32_t lenSlot = declareLocal("$len_" + std::to_string(tempCounter_++));
        int32_t varSlot = declareLocal(fe->varName);

        emitExpr(*fe->iterable);
        curFn().emit(OpCode::DUP, 0, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, iterSlot, 0, line);
        curFn().emit(OpCode::ITER_LEN, 0, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, lenSlot, 0, line);

        int32_t zeroConst = curFn().addConstant(Constant::cijeli(0));
        curFn().emit(OpCode::PUSH_CONST, zeroConst, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, idxSlot, 0, line);

        int32_t condIp = static_cast<int32_t>(curFn().code.size());
        curFn().emit(OpCode::LOAD_LOCAL, idxSlot, 0, line);
        curFn().emit(OpCode::LOAD_LOCAL, lenSlot, 0, line);
        curFn().emit(OpCode::LT, 0, 0, line);
        int32_t exitJump = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, line);

        curFn().emit(OpCode::LOAD_LOCAL, iterSlot, 0, line);
        curFn().emit(OpCode::LOAD_LOCAL, idxSlot, 0, line);
        curFn().emit(OpCode::ITER_GET, 0, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, varSlot, 0, line);

        loopStack_.push_back(LoopContext{});
        pushLocalScope();
        for (const auto& s : fe->body) emitStmt(*s);
        popLocalScope();
        LoopContext ctx = std::move(loopStack_.back());
        loopStack_.pop_back();

        int32_t stepIp = static_cast<int32_t>(curFn().code.size());
        for (int32_t cj : ctx.continueJumps) {
            curFn().code[cj].a = stepIp;
        }
        int32_t oneConst = curFn().addConstant(Constant::cijeli(1));
        curFn().emit(OpCode::LOAD_LOCAL, idxSlot, 0, line);
        curFn().emit(OpCode::PUSH_CONST, oneConst, 0, line);
        curFn().emit(OpCode::ADD, 0, 0, line);
        curFn().emit(OpCode::STORE_LOCAL, idxSlot, 0, line);
        curFn().emit(OpCode::JUMP, condIp, 0, line);

        int32_t loopEnd = static_cast<int32_t>(curFn().code.size());
        curFn().code[exitJump].a = loopEnd;
        for (int32_t bj : ctx.breakJumps) {
            curFn().code[bj].a = loopEnd;
        }
        popLocalScope();
        return;
    }

    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (!loopStack_.empty()) {
            int32_t j = curFn().emit(OpCode::JUMP, 0, 0, line);
            loopStack_.back().breakJumps.push_back(j);
        }
        return;
    }

    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (!loopStack_.empty()) {
            int32_t j = curFn().emit(OpCode::JUMP, 0, 0, line);
            loopStack_.back().continueJumps.push_back(j);
        }
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
        if (r->value) {
            emitExpr(*r->value);
        } else {
            curFn().emit(OpCode::PUSH_PRAZNO, 0, 0, line);
        }
        curFn().emit(OpCode::RETURN, 0, 0, line);
        return;
    }

    if (auto* tc = dynamic_cast<const TryCatchStmt*>(&stmt)) {
        int32_t pushTry = curFn().emit(OpCode::PUSH_TRY, 0, 0, line);

        pushLocalScope();
        for (const auto& s : tc->tryBody) emitStmt(*s);
        popLocalScope();

        curFn().emit(OpCode::POP_TRY, 0, 0, line);
        int32_t jumpEnd = curFn().emit(OpCode::JUMP, 0, 0, line);

        int32_t catchIp = static_cast<int32_t>(curFn().code.size());
        curFn().code[pushTry].a = catchIp;

        pushLocalScope();
        int32_t errSlot = declareLocal(tc->catchVar);
        curFn().emit(OpCode::STORE_LOCAL, errSlot, 0, line);
        for (const auto& s : tc->catchBody) emitStmt(*s);
        popLocalScope();

        curFn().code[jumpEnd].a = static_cast<int32_t>(curFn().code.size());
        return;
    }

    if (auto* th = dynamic_cast<const ThrowStmt*>(&stmt)) {
        emitExpr(*th->expr);
        curFn().emit(OpCode::THROW, 0, 0, line);
        return;
    }

    if (auto* imp = dynamic_cast<const ImportStmt*>(&stmt)) {
        int32_t mc = curFn().addConstant(Constant::tekst(imp->moduleName));
        int32_t g = getOrAddGlobal(imp->moduleName);
        curFn().emit(OpCode::IMPORT_MODULE, mc, g, line);
        return;
    }
}

void IRGenerator::emitExpr(const Expr& expr) {
    int32_t line = expr.location.line;

    if (auto* i = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        int32_t c = curFn().addConstant(Constant::cijeli(i->value));
        curFn().emit(OpCode::PUSH_CONST, c, 0, line);
        return;
    }
    if (auto* f = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        int32_t c = curFn().addConstant(Constant::decimalni(f->value));
        curFn().emit(OpCode::PUSH_CONST, c, 0, line);
        return;
    }
    if (auto* s = dynamic_cast<const StringLiteralExpr*>(&expr)) {
        int32_t c = curFn().addConstant(Constant::tekst(s->value));
        curFn().emit(OpCode::PUSH_CONST, c, 0, line);
        return;
    }
    if (auto* ch = dynamic_cast<const CharLiteralExpr*>(&expr)) {
        int32_t c = curFn().addConstant(Constant::znak(ch->value));
        curFn().emit(OpCode::PUSH_CONST, c, 0, line);
        return;
    }
    if (auto* b = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        int32_t c = curFn().addConstant(Constant::logicki(b->value));
        curFn().emit(OpCode::PUSH_CONST, c, 0, line);
        return;
    }
    if (dynamic_cast<const SelfExpr*>(&expr)) {
        curFn().emit(OpCode::LOAD_SELF, 0, 0, line);
        return;
    }
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        if (id->resolvedAsSelfField) {
            int32_t fc = curFn().addConstant(Constant::tekst(id->name));
            curFn().emit(OpCode::LOAD_SELF, 0, 0, line);
            curFn().emit(OpCode::GET_FIELD, fc, 0, line);
            return;
        }
        int32_t local = lookupLocal(id->name);
        if (local >= 0) {
            curFn().emit(OpCode::LOAD_LOCAL, local, 0, line);
        } else {
            int32_t g = getOrAddGlobal(id->name);
            curFn().emit(OpCode::LOAD_GLOBAL, g, 0, line);
        }
        return;
    }
    if (auto* un = dynamic_cast<const UnaryExpr*>(&expr)) {
        emitExpr(*un->operand);
        if (un->op == UnaryOp::Negate) curFn().emit(OpCode::NEG, 0, 0, line);
        else if (un->op == UnaryOp::Not) curFn().emit(OpCode::NOT, 0, 0, line);
        return;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        if (bin->op == BinaryOp::And) {
            emitExpr(*bin->left);
            curFn().emit(OpCode::DUP, 0, 0, line);
            int32_t skip = curFn().emit(OpCode::JUMP_IF_FALSE, 0, 0, line);
            curFn().emit(OpCode::POP, 0, 0, line);
            emitExpr(*bin->right);
            curFn().code[skip].a = static_cast<int32_t>(curFn().code.size());
            return;
        }
        if (bin->op == BinaryOp::Or) {
            emitExpr(*bin->left);
            curFn().emit(OpCode::DUP, 0, 0, line);
            int32_t skip = curFn().emit(OpCode::JUMP_IF_TRUE, 0, 0, line);
            curFn().emit(OpCode::POP, 0, 0, line);
            emitExpr(*bin->right);
            curFn().code[skip].a = static_cast<int32_t>(curFn().code.size());
            return;
        }
        emitExpr(*bin->left);
        emitExpr(*bin->right);
        switch (bin->op) {
            case BinaryOp::Add: curFn().emit(OpCode::ADD, 0, 0, line); break;
            case BinaryOp::Sub: curFn().emit(OpCode::SUB, 0, 0, line); break;
            case BinaryOp::Mul: curFn().emit(OpCode::MUL, 0, 0, line); break;
            case BinaryOp::Div: curFn().emit(OpCode::DIV, 0, 0, line); break;
            case BinaryOp::Mod: curFn().emit(OpCode::MOD, 0, 0, line); break;
            case BinaryOp::Eq: curFn().emit(OpCode::EQ, 0, 0, line); break;
            case BinaryOp::Neq: curFn().emit(OpCode::NEQ, 0, 0, line); break;
            case BinaryOp::Lt: curFn().emit(OpCode::LT, 0, 0, line); break;
            case BinaryOp::Lte: curFn().emit(OpCode::LTE, 0, 0, line); break;
            case BinaryOp::Gt: curFn().emit(OpCode::GT, 0, 0, line); break;
            case BinaryOp::Gte: curFn().emit(OpCode::GTE, 0, 0, line); break;
            default: break;
        }
        return;
    }
    if (auto* mem = dynamic_cast<const MemberAccessExpr*>(&expr)) {
        if (mem->object->inferredType && mem->object->inferredType->kind == TypeKind::Modul) {
            int32_t mc = curFn().addConstant(Constant::tekst(mem->object->inferredType->name + "." + mem->member));
            curFn().emit(OpCode::GET_MODULE_CONST, mc, 0, line);
            return;
        }
        emitExpr(*mem->object);
        int32_t fc = curFn().addConstant(Constant::tekst(mem->member));
        curFn().emit(OpCode::GET_FIELD, fc, 0, line);
        return;
    }
    if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        emitExpr(*idx->object);
        emitExpr(*idx->index);
        curFn().emit(OpCode::GET_INDEX, 0, 0, line);
        return;
    }
    if (auto* list = dynamic_cast<const ListLiteralExpr*>(&expr)) {
        for (const auto& el : list->elements) emitExpr(*el);
        curFn().emit(list->isNiz ? OpCode::BUILD_NIZ : OpCode::BUILD_LISTA,
                     static_cast<int32_t>(list->elements.size()), 0, line);
        return;
    }
    if (auto* dict = dynamic_cast<const DictLiteralExpr*>(&expr)) {
        for (const auto& kv : dict->entries) {
            emitExpr(*kv.first);
            emitExpr(*kv.second);
        }
        curFn().emit(OpCode::BUILD_RJECNIK, static_cast<int32_t>(dict->entries.size()), 0, line);
        return;
    }
    if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        int32_t argc = static_cast<int32_t>(call->args.size());
        if (auto* id = dynamic_cast<const IdentifierExpr*>(call->callee.get())) {
            if (id->resolvedAsSelfMethod) {
                curFn().emit(OpCode::LOAD_SELF, 0, 0, line);
                for (const auto& a : call->args) emitExpr(*a);
                int32_t mc = curFn().addConstant(Constant::tekst(id->name));
                curFn().emit(OpCode::CALL_METHOD, mc, argc, line);
                return;
            }
            int32_t stIdx = module_.findStruct(id->name);
            if (stIdx >= 0) {
                for (const auto& a : call->args) emitExpr(*a);
                curFn().emit(OpCode::NEW_STRUCT, stIdx, argc, line);
                return;
            }
            int32_t clIdx = module_.findClass(id->name);
            if (clIdx >= 0) {
                for (const auto& a : call->args) emitExpr(*a);
                curFn().emit(OpCode::NEW_CLASS, clIdx, argc, line);
                return;
            }
            int32_t fnIdx = module_.findFunction(id->name);
            if (fnIdx >= 0) {
                for (const auto& a : call->args) emitExpr(*a);
                curFn().emit(OpCode::CALL_FUNC, fnIdx, argc, line);
                return;
            }
        } else if (auto* mem = dynamic_cast<const MemberAccessExpr*>(call->callee.get())) {
            TypePtr objType = mem->object->inferredType;
            if (objType && objType->kind == TypeKind::Modul) {
                for (const auto& a : call->args) emitExpr(*a);
                int32_t mc = curFn().addConstant(Constant::tekst(objType->name + "." + mem->member));
                curFn().emit(OpCode::CALL_MODULE, mc, argc, line);
                return;
            }
            emitExpr(*mem->object);
            for (const auto& a : call->args) emitExpr(*a);
            int32_t mc = curFn().addConstant(Constant::tekst(mem->member));
            if (objType && objType->kind == TypeKind::Klasa) {
                curFn().emit(OpCode::CALL_METHOD, mc, argc, line);
            } else {
                curFn().emit(OpCode::CALL_BUILTIN, mc, argc, line);
            }
            return;
        }
        curFn().emit(OpCode::PUSH_PRAZNO, 0, 0, line);
        return;
    }
}

} // namespace bos
