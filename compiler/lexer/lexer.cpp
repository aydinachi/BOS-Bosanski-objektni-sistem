#include "lexer/lexer.hpp"

#include <cstdlib>
#include <sstream>

namespace bos {

Lexer::Lexer(std::string source, std::string filename, DiagnosticEngine& diagnostics)
    : source_(std::move(source)), filename_(std::move(filename)), diagnostics_(diagnostics) {
    // Strip UTF-8 BOM if present
    if (source_.size() >= 3 &&
        static_cast<unsigned char>(source_[0]) == 0xEF &&
        static_cast<unsigned char>(source_[1]) == 0xBB &&
        static_cast<unsigned char>(source_[2]) == 0xBF) {
        pos_ = 3;
    }
}

bool Lexer::decodeUtf8(const std::string& str, size_t pos, char32_t& outCp, size_t& outLen) {
    if (pos >= str.size()) return false;
    unsigned char b0 = static_cast<unsigned char>(str[pos]);
    if (b0 < 0x80) {
        outCp = b0;
        outLen = 1;
        return true;
    }
    if ((b0 & 0xE0) == 0xC0) {
        if (pos + 1 >= str.size()) return false;
        unsigned char b1 = static_cast<unsigned char>(str[pos + 1]);
        if ((b1 & 0xC0) != 0x80) return false;
        outCp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        outLen = 2;
        return true;
    }
    if ((b0 & 0xF0) == 0xE0) {
        if (pos + 2 >= str.size()) return false;
        unsigned char b1 = static_cast<unsigned char>(str[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(str[pos + 2]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
        outCp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        outLen = 3;
        return true;
    }
    if ((b0 & 0xF8) == 0xF0) {
        if (pos + 3 >= str.size()) return false;
        unsigned char b1 = static_cast<unsigned char>(str[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(str[pos + 2]);
        unsigned char b3 = static_cast<unsigned char>(str[pos + 3]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
        outCp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        outLen = 4;
        return true;
    }
    return false;
}

std::string Lexer::encodeUtf8(char32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

bool Lexer::isIdentifierStart(char32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_') {
        return true;
    }
    // Bosnian Latin Extended-A diacritics: č Č ć Ć ž Ž š Š đ Đ
    if (cp == 0x010C || cp == 0x010D || // Č, č
        cp == 0x0106 || cp == 0x0107 || // Ć, ć
        cp == 0x017D || cp == 0x017E || // Ž, ž
        cp == 0x0160 || cp == 0x0161 || // Š, š
        cp == 0x0110 || cp == 0x0111) { // Đ, đ
        return true;
    }
    // Allow general Latin-1 Supplement / Latin Extended letters for broad UTF-8 identifiers
    if ((cp >= 0x00C0 && cp <= 0x00D6) ||
        (cp >= 0x00D8 && cp <= 0x00F6) ||
        (cp >= 0x00F8 && cp <= 0x024F)) {
        return true;
    }
    return false;
}

bool Lexer::isIdentifierContinue(char32_t cp) {
    return isIdentifierStart(cp) || (cp >= '0' && cp <= '9');
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

char Lexer::peekByte(size_t offset) const {
    if (pos_ + offset >= source_.size()) return '\0';
    return source_[pos_ + offset];
}

char32_t Lexer::peekCodepoint(size_t* byteLen) const {
    char32_t cp = 0;
    size_t len = 1;
    if (!decodeUtf8(source_, pos_, cp, len)) {
        if (byteLen) *byteLen = 1;
        return static_cast<unsigned char>(source_[pos_]);
    }
    if (byteLen) *byteLen = len;
    return cp;
}

char32_t Lexer::advanceCodepoint() {
    size_t len = 1;
    char32_t cp = peekCodepoint(&len);
    pos_ += len;
    if (cp == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return cp;
}

bool Lexer::matchByte(char expected) {
    if (isAtEnd() || source_[pos_] != expected) return false;
    advanceCodepoint();
    return true;
}

void Lexer::skipSpacesAndComments() {
    while (!isAtEnd()) {
        char c = peekByte();
        if (c == ' ' || c == '\t' || c == '\r') {
            advanceCodepoint();
        } else if (c == '/' && peekByte(1) == '/') {
            while (!isAtEnd() && peekByte() != '\n') {
                advanceCodepoint();
            }
        } else if (c == '/' && peekByte(1) == '*') {
            SourceLocation startLoc{filename_, line_, column_};
            advanceCodepoint(); // /
            advanceCodepoint(); // *
            bool closed = false;
            while (!isAtEnd()) {
                if (peekByte() == '*' && peekByte(1) == '/') {
                    advanceCodepoint();
                    advanceCodepoint();
                    closed = true;
                    break;
                }
                advanceCodepoint();
            }
            if (!closed) {
                diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                                   "Nezatvoren višelinijski komentar '/* ... */'.", "L001");
            }
        } else {
            break;
        }
    }
}

Token Lexer::scanString(const SourceLocation& startLoc) {
    std::string value;
    advanceCodepoint(); // opening "
    bool closed = false;
    while (!isAtEnd()) {
        char c = peekByte();
        if (c == '"') {
            advanceCodepoint();
            closed = true;
            break;
        }
        if (c == '\n') {
            break;
        }
        if (c == '\\') {
            advanceCodepoint();
            if (isAtEnd()) break;
            char esc = peekByte();
            advanceCodepoint();
            switch (esc) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '0': value.push_back('\0'); break;
                default:
                    diagnostics_.error(DiagnosticStage::Lexer, SourceLocation{filename_, line_, column_},
                                       std::string("Nepoznata izlazna sekvenca '\\") + esc + "' u tekstu.", "L002");
                    value.push_back(esc);
                    break;
            }
        } else {
            char32_t cp = advanceCodepoint();
            value += encodeUtf8(cp);
        }
    }

    if (!closed) {
        diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                           "Nezatvoren tekstualni literal (nedostaje završni navodnik '\"').", "L003");
        return Token{TokenType::Invalid, value, startLoc, 0, 0.0, 0};
    }

    return Token{TokenType::StringLiteral, value, startLoc, 0, 0.0, 0};
}

Token Lexer::scanChar(const SourceLocation& startLoc) {
    advanceCodepoint(); // opening '
    if (isAtEnd() || peekByte() == '\n') {
        diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                           "Nezatvoren znakovni literal.", "L004");
        return Token{TokenType::Invalid, "", startLoc, 0, 0.0, 0};
    }

    char32_t cp = 0;
    if (peekByte() == '\\') {
        advanceCodepoint();
        if (isAtEnd()) {
            diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                               "Nezatvoren znakovni literal.", "L004");
            return Token{TokenType::Invalid, "", startLoc, 0, 0.0, 0};
        }
        char esc = peekByte();
        advanceCodepoint();
        switch (esc) {
            case 'n': cp = '\n'; break;
            case 't': cp = '\t'; break;
            case 'r': cp = '\r'; break;
            case '\'': cp = '\''; break;
            case '\\': cp = '\\'; break;
            case '0': cp = '\0'; break;
            default:
                diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                                   std::string("Nepoznata izlazna sekvenca '\\") + esc + "' u znakovnom literalu.", "L005");
                cp = static_cast<unsigned char>(esc);
                break;
        }
    } else if (peekByte() == '\'') {
        advanceCodepoint();
        diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                           "Prazan znakovni literal nije dozvoljen.", "L006");
        return Token{TokenType::Invalid, "", startLoc, 0, 0.0, 0};
    } else {
        cp = advanceCodepoint();
    }

    if (isAtEnd() || peekByte() != '\'') {
        diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                           "Znakovni literal mora sadržavati tačno jedan znak i završavati sa \"'\".", "L007");
        while (!isAtEnd() && peekByte() != '\'' && peekByte() != '\n') {
            advanceCodepoint();
        }
        if (!isAtEnd() && peekByte() == '\'') advanceCodepoint();
        return Token{TokenType::Invalid, encodeUtf8(cp), startLoc, 0, 0.0, cp};
    }

    advanceCodepoint(); // closing '
    return Token{TokenType::CharLiteral, encodeUtf8(cp), startLoc, 0, 0.0, cp};
}

Token Lexer::scanNumber(const SourceLocation& startLoc) {
    size_t startPos = pos_;
    bool isFloat = false;

    while (!isAtEnd() && peekByte() >= '0' && peekByte() <= '9') {
        advanceCodepoint();
    }

    if (!isAtEnd() && peekByte() == '.' && peekByte(1) >= '0' && peekByte(1) <= '9') {
        isFloat = true;
        advanceCodepoint(); // '.'
        while (!isAtEnd() && peekByte() >= '0' && peekByte() <= '9') {
            advanceCodepoint();
        }
    }

    std::string lexeme = source_.substr(startPos, pos_ - startPos);
    Token tok;
    tok.lexeme = lexeme;
    tok.location = startLoc;

    if (isFloat) {
        tok.type = TokenType::FloatLiteral;
        try {
            tok.floatValue = std::stod(lexeme);
        } catch (...) {
            diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                               "Neispravan decimalni broj: '" + lexeme + "'.", "L008");
            tok.type = TokenType::Invalid;
        }
    } else {
        tok.type = TokenType::IntLiteral;
        try {
            tok.intValue = std::stoll(lexeme);
        } catch (...) {
            diagnostics_.error(DiagnosticStage::Lexer, startLoc,
                               "Cijeli broj je izvan dozvoljenog opsega: '" + lexeme + "'.", "L009");
            tok.type = TokenType::Invalid;
        }
    }
    return tok;
}

Token Lexer::scanIdentifierOrKeyword(const SourceLocation& startLoc) {
    size_t startPos = pos_;
    while (!isAtEnd()) {
        size_t len = 1;
        char32_t cp = peekCodepoint(&len);
        if (!isIdentifierContinue(cp)) break;
        advanceCodepoint();
    }

    std::string lexeme = source_.substr(startPos, pos_ - startPos);
    TokenType type = lookupKeyword(lexeme);
    return Token{type, lexeme, startLoc, 0, 0.0, 0};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipSpacesAndComments();
        if (isAtEnd()) break;

        SourceLocation loc{filename_, line_, column_};
        char c = peekByte();

        if (c == '\n') {
            advanceCodepoint();
            // Collapse consecutive newlines so parser sees clean statement boundaries
            if (tokens.empty() || tokens.back().type != TokenType::Newline) {
                tokens.push_back(Token{TokenType::Newline, "\\n", loc, 0, 0.0, 0});
            }
            continue;
        }

        if (c == '"') {
            tokens.push_back(scanString(loc));
            continue;
        }

        if (c == '\'') {
            tokens.push_back(scanChar(loc));
            continue;
        }

        if (c >= '0' && c <= '9') {
            tokens.push_back(scanNumber(loc));
            continue;
        }

        size_t cpLen = 1;
        char32_t cp = peekCodepoint(&cpLen);
        if (isIdentifierStart(cp)) {
            tokens.push_back(scanIdentifierOrKeyword(loc));
            continue;
        }

        // Operators and delimiters
        advanceCodepoint();
        switch (c) {
            case '+':
                if (matchByte('=')) tokens.push_back({TokenType::PlusEqual, "+=", loc});
                else tokens.push_back({TokenType::Plus, "+", loc});
                break;
            case '-':
                if (matchByte('=')) tokens.push_back({TokenType::MinusEqual, "-=", loc});
                else tokens.push_back({TokenType::Minus, "-", loc});
                break;
            case '*':
                if (matchByte('=')) tokens.push_back({TokenType::StarEqual, "*=", loc});
                else tokens.push_back({TokenType::Star, "*", loc});
                break;
            case '/':
                if (matchByte('=')) tokens.push_back({TokenType::SlashEqual, "/=", loc});
                else tokens.push_back({TokenType::Slash, "/", loc});
                break;
            case '%':
                tokens.push_back({TokenType::Percent, "%", loc});
                break;
            case '=':
                if (matchByte('=')) tokens.push_back({TokenType::EqualEqual, "==", loc});
                else tokens.push_back({TokenType::Equal, "=", loc});
                break;
            case '!':
                if (matchByte('=')) {
                    tokens.push_back({TokenType::BangEqual, "!=", loc});
                } else {
                    diagnostics_.error(DiagnosticStage::Lexer, loc,
                                       "Neočekivan znak '!'. Za logičku negaciju u BOS-u koristite ključnu riječ 'ne' ili operator '!='.", "L010");
                    tokens.push_back({TokenType::Invalid, "!", loc});
                }
                break;
            case '<':
                if (matchByte('=')) tokens.push_back({TokenType::LessEqual, "<=", loc});
                else tokens.push_back({TokenType::Less, "<", loc});
                break;
            case '>':
                if (matchByte('=')) tokens.push_back({TokenType::GreaterEqual, ">=", loc});
                else tokens.push_back({TokenType::Greater, ">", loc});
                break;
            case '(': tokens.push_back({TokenType::LParen, "(", loc}); break;
            case ')': tokens.push_back({TokenType::RParen, ")", loc}); break;
            case '[': tokens.push_back({TokenType::LBracket, "[", loc}); break;
            case ']': tokens.push_back({TokenType::RBracket, "]", loc}); break;
            case '{': tokens.push_back({TokenType::LBrace, "{", loc}); break;
            case '}': tokens.push_back({TokenType::RBrace, "}", loc}); break;
            case ',': tokens.push_back({TokenType::Comma, ",", loc}); break;
            case ':': tokens.push_back({TokenType::Colon, ":", loc}); break;
            case '.': tokens.push_back({TokenType::Dot, ".", loc}); break;
            case ';': tokens.push_back({TokenType::Semicolon, ";", loc}); break;
            default: {
                std::string bad = encodeUtf8(cp);
                diagnostics_.error(DiagnosticStage::Lexer, loc,
                                   "Nepoznat znak '" + bad + "' u izvornom kodu.", "L011");
                tokens.push_back({TokenType::Invalid, bad, loc});
                break;
            }
        }
    }

    tokens.push_back(Token{TokenType::EndOfFile, "", SourceLocation{filename_, line_, column_}, 0, 0.0, 0});
    return tokens;
}

} // namespace bos
