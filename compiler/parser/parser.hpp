#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostic.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <string>
#include <vector>

namespace bos {

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string filename, DiagnosticEngine& diagnostics);

    Program parseProgram();

private:
    const Token& peek(size_t offset = 0) const;
    const Token& previous() const;
    bool isAtEnd() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    void skipNewlines();
    bool consumeEndOfStatement();
    const Token& expect(TokenType type, const std::string& message, const std::string& code);
    bool isIdentifierToken(TokenType type) const;
    bool isMemberNameToken(TokenType type) const;
    std::string expectIdentifier(const std::string& contextMessage, const std::string& code);
    void synchronize();

    TypeAnnotation parseTypeAnnotation();

    // Statements
    StmtPtr parseStatement();
    StmtPtr parseVarDecl(bool isConst);
    StmtPtr parsePrintStmt();
    StmtPtr parseInputStmt();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForRangeStmt();
    StmtPtr parseForEachStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseTryCatchStmt();
    StmtPtr parseThrowStmt();
    StmtPtr parseImportStmt();
    std::unique_ptr<FuncDeclStmt> parseFuncDecl(Visibility vis, bool isConstructor);
    StmtPtr parseStructDecl();
    StmtPtr parseClassDecl();
    StmtPtr parseExprOrAssignStmt();

    std::vector<StmtPtr> parseBlockUntil(const std::vector<TokenType>& stopTokens);

    // Expressions (precedence climbing / recursive descent)
    ExprPtr parseExpression();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();

    std::vector<Token> tokens_;
    std::string filename_;
    DiagnosticEngine& diagnostics_;
    size_t current_ = 0;
};

} // namespace bos
