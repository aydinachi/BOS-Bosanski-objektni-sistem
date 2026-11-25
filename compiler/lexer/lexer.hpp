#pragma once

#include "diagnostics/diagnostic.hpp"
#include "lexer/token.hpp"

#include <string>
#include <vector>

namespace bos {

class Lexer {
public:
    Lexer(std::string source, std::string filename, DiagnosticEngine& diagnostics);

    std::vector<Token> tokenize();

    static bool decodeUtf8(const std::string& str, size_t pos, char32_t& outCp, size_t& outLen);
    static std::string encodeUtf8(char32_t cp);
    static bool isIdentifierStart(char32_t cp);
    static bool isIdentifierContinue(char32_t cp);

private:
    bool isAtEnd() const;
    char peekByte(size_t offset = 0) const;
    char32_t peekCodepoint(size_t* byteLen = nullptr) const;
    char32_t advanceCodepoint();
    bool matchByte(char expected);

    void skipSpacesAndComments();
    Token scanString(const SourceLocation& startLoc);
    Token scanChar(const SourceLocation& startLoc);
    Token scanNumber(const SourceLocation& startLoc);
    Token scanIdentifierOrKeyword(const SourceLocation& startLoc);

    std::string source_;
    std::string filename_;
    DiagnosticEngine& diagnostics_;

    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

} // namespace bos
