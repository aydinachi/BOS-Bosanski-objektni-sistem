#pragma once

#include "diagnostics/diagnostic.hpp"
#include "types/type.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bos {

struct TypeAnnotation {
    std::string name;
    std::vector<TypeAnnotation> typeArgs;
    SourceLocation location;
};

enum class Visibility {
    Javno,
    Privatno
};

enum class UnaryOp {
    Negate,
    Plus,
    Not
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte,
    And,
    Or
};

enum class AssignOp {
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign
};

struct Expr {
    SourceLocation location;
    TypePtr inferredType;
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct Stmt {
    SourceLocation location;
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

// ===================== Expressions =====================

struct IntLiteralExpr : Expr {
    int64_t value = 0;
};

struct FloatLiteralExpr : Expr {
    double value = 0.0;
};

struct StringLiteralExpr : Expr {
    std::string value;
};

struct CharLiteralExpr : Expr {
    char32_t value = 0;
    std::string utf8;
};

struct BoolLiteralExpr : Expr {
    bool value = false;
};

struct IdentifierExpr : Expr {
    std::string name;
    bool resolvedAsSelfField = false;   // Set by semantic analyzer when inside a class method
    bool resolvedAsSelfMethod = false;  // Set by semantic analyzer when inside a class method
};

struct SelfExpr : Expr {};

struct UnaryExpr : Expr {
    UnaryOp op = UnaryOp::Negate;
    ExprPtr operand;
};

struct BinaryExpr : Expr {
    BinaryOp op = BinaryOp::Add;
    ExprPtr left;
    ExprPtr right;
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

struct MemberAccessExpr : Expr {
    ExprPtr object;
    std::string member;
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
};

struct ListLiteralExpr : Expr {
    std::vector<ExprPtr> elements;
    bool isNiz = false; // Set by semantic analyzer when assigned to niz<T>
};

struct DictLiteralExpr : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
};

// ===================== Statements =====================

struct VarDeclStmt : Stmt {
    bool isConst = false;
    std::string name;
    std::optional<TypeAnnotation> typeAnn;
    ExprPtr initializer; // optional when typeAnn is provided
    TypePtr resolvedType;
};

struct AssignStmt : Stmt {
    ExprPtr target; // IdentifierExpr, MemberAccessExpr, or IndexExpr
    AssignOp op = AssignOp::Assign;
    ExprPtr value;
};

struct ExprStmt : Stmt {
    ExprPtr expr;
};

struct PrintStmt : Stmt {
    ExprPtr expr;
};

struct InputStmt : Stmt {
    std::string targetName;
    TypePtr targetType;
    bool resolvedAsSelfField = false;
};

struct ElseIfBranch {
    SourceLocation location;
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct IfStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> thenBranch;
    std::vector<ElseIfBranch> elseIfBranches;
    std::vector<StmtPtr> elseBranch;
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct ForRangeStmt : Stmt {
    std::string varName;
    ExprPtr start;
    ExprPtr end;
    std::vector<StmtPtr> body;
};

struct ForEachStmt : Stmt {
    std::string varName;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
    TypePtr elementType;
};

struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};

struct ReturnStmt : Stmt {
    ExprPtr value; // nullptr for bare `vrati`
};

struct TryCatchStmt : Stmt {
    std::vector<StmtPtr> tryBody;
    std::string catchVar;
    std::vector<StmtPtr> catchBody;
};

struct ThrowStmt : Stmt {
    ExprPtr expr;
};

struct ImportStmt : Stmt {
    std::string moduleName;
};

struct ParamDecl {
    std::string name;
    TypeAnnotation typeAnn;
    TypePtr resolvedType;
    SourceLocation location;
};

struct FuncDeclStmt : Stmt {
    std::string name;
    std::vector<ParamDecl> params;
    TypeAnnotation returnTypeAnn;
    TypePtr resolvedReturnType;
    std::vector<StmtPtr> body;
    Visibility visibility = Visibility::Javno;
    bool isConstructor = false;
};

struct FieldDecl {
    std::string name;
    TypeAnnotation typeAnn;
    TypePtr resolvedType;
    ExprPtr defaultValue; // optional
    Visibility visibility = Visibility::Javno;
    SourceLocation location;
};

struct StructDeclStmt : Stmt {
    std::string name;
    std::vector<FieldDecl> fields;
};

struct ClassDeclStmt : Stmt {
    std::string name;
    std::string baseClass; // empty if no inheritance
    std::vector<FieldDecl> fields;
    std::vector<std::unique_ptr<FuncDeclStmt>> methods;
};

struct Program {
    std::string filename;
    std::vector<StmtPtr> statements;
};

} // namespace bos
