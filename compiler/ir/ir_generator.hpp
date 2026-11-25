#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostic.hpp"
#include "ir/ir.hpp"
#include "semantic/symbol_table.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace bos {

class IRGenerator {
public:
    IRGenerator(const SymbolTable& symbols, DiagnosticEngine& diagnostics);

    BytecodeModule generate(const Program& program);

private:
    struct LoopContext {
        std::vector<int32_t> breakJumps;
        std::vector<int32_t> continueJumps;
    };

    FunctionIR& curFn() { return module_.functions[curFuncIdx_]; }

    int32_t getOrAddGlobal(const std::string& name);
    int32_t lookupGlobal(const std::string& name) const;
    int32_t declareLocal(const std::string& name);
    int32_t lookupLocal(const std::string& name) const;
    void pushLocalScope();
    void popLocalScope();

    uint8_t typeToDefaultTag(const TypePtr& type) const;
    int32_t typeToInputTag(const TypePtr& type) const;
    void emitDefaultValue(const TypePtr& type, int32_t line);

    void registerMetadata(const Program& program);
    void compileFunction(const FuncDeclStmt& fn, int32_t funcIndex, const std::string& ownerClass);
    void emitStmt(const Stmt& stmt);
    void emitExpr(const Expr& expr);
    void emitCompoundOp(AssignOp op, int32_t line);

    const SymbolTable& symbols_;
    DiagnosticEngine& diagnostics_;
    BytecodeModule module_;

    int32_t curFuncIdx_ = 0;
    bool inTopLevel_ = true;
    std::string curClass_;

    std::unordered_map<std::string, int32_t> globalMap_;
    std::vector<std::unordered_map<std::string, int32_t>> localScopes_;
    int32_t nextLocalSlot_ = 0;
    int32_t maxLocalSlot_ = 0;
    int32_t tempCounter_ = 0;

    std::vector<LoopContext> loopStack_;
    std::unordered_map<std::string, int32_t> funcIndexMap_; // name or Class::method -> funcIdx
};

} // namespace bos
