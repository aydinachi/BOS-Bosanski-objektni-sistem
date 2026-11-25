#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostic.hpp"
#include "semantic/symbol_table.hpp"

#include <string>
#include <unordered_set>

namespace bos {

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(DiagnosticEngine& diagnostics);

    void analyze(Program& program);
    const SymbolTable& symbols() const { return symbols_; }

private:
    TypePtr resolveTypeAnnotation(const TypeAnnotation& ann);
    bool isAssignable(const TypePtr& target, const TypePtr& source) const;

    // Two-pass top-level registration
    void registerTypeNames(Program& program);
    void registerTypeMembersAndFunctions(Program& program);
    void validateInheritance(Program& program);

    // Statement analysis
    void analyzeStmt(Stmt& stmt);
    void analyzeVarDecl(VarDeclStmt& stmt);
    void analyzeAssign(AssignStmt& stmt);
    void analyzeIf(IfStmt& stmt);
    void analyzeWhile(WhileStmt& stmt);
    void analyzeForRange(ForRangeStmt& stmt);
    void analyzeForEach(ForEachStmt& stmt);
    void analyzeReturn(ReturnStmt& stmt);
    void analyzeTryCatch(TryCatchStmt& stmt);
    void analyzeImport(ImportStmt& stmt);
    void analyzeFuncBody(FuncDeclStmt& fn, const std::string& ownerClass);
    void analyzeClassBody(ClassDeclStmt& cl);

    // Expression analysis
    TypePtr analyzeExpr(Expr& expr, TypePtr expectedType = nullptr);
    TypePtr analyzeCall(CallExpr& call);
    TypePtr analyzeMemberAccess(MemberAccessExpr& mem);
    TypePtr analyzeModuleCall(const std::string& modName, const std::string& funcName,
                              CallExpr& call);
    TypePtr analyzeBuiltinMethodCall(const TypePtr& objType, const std::string& methodName,
                                     CallExpr& call);

    DiagnosticEngine& diagnostics_;
    SymbolTable symbols_;
    std::unordered_set<std::string> importedModules_;

    std::string currentClass_;
    TypePtr currentReturnType_;
    bool inFunction_ = false;
    bool inConstructor_ = false;
    int loopDepth_ = 0;
};

} // namespace bos
