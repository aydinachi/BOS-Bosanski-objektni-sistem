#pragma once

#include "ast/ast.hpp"

#include <string>

namespace bos {

class AstPrinter {
public:
    static std::string print(const Program& program);

private:
    void printStmt(const Stmt& stmt, int indent);
    void printExpr(const Expr& expr, int indent);
    void pad(int indent);
    static std::string formatTypeAnn(const TypeAnnotation& ann);
    static const char* binaryOpName(BinaryOp op);
    static const char* unaryOpName(UnaryOp op);
    static const char* assignOpName(AssignOp op);

    std::string out_;
};

} // namespace bos
