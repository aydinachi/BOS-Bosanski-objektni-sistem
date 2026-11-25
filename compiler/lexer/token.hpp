#pragma once

#include "diagnostics/diagnostic.hpp"

#include <cstdint>
#include <string>

namespace bos {

enum class TokenType {
    // End / structure
    EndOfFile,
    Newline,

    // Literals & identifiers
    Identifier,
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,

    // Keywords: declarations & types
    KwNeka,         // neka
    KwKao,          // kao
    KwStalno,       // stalno
    KwFunkcija,     // funkcija
    KwVraca,        // vraća / vraca
    KwVrati,        // vrati
    KwZapis,        // zapis
    KwKlasa,        // klasa
    KwNasljeduje,   // nasljeduje / nasljeđuje
    KwJavno,        // javno
    KwPrivatno,     // privatno
    KwKonstruktor,  // konstruktor
    KwOvaj,         // ovaj

    // Primitive & generic type keywords
    KwCijeli,       // cijeli
    KwDecimalni,    // decimalni
    KwTekst,        // tekst
    KwZnak,         // znak
    KwLogicki,      // logički / logicki
    KwPrazno,       // prazno
    KwNiz,          // niz
    KwLista,        // lista
    KwRjecnik,      // rjecnik / rječnik

    // Control flow
    KwAko,          // ako
    KwOnda,         // onda
    KwInace,        // inače / inace
    KwKraj,         // kraj
    KwPonavljaj,    // ponavljaj
    KwOd,           // od
    KwDo,           // do
    KwZa,           // za
    KwSvaki,        // svaki
    KwU,            // u
    KwDok,          // dok
    KwRadi,         // radi
    KwPrekini,      // prekini
    KwNastavi,      // nastavi

    // Boolean literals & logical operators
    KwTacno,        // tačno / tacno
    KwNetacno,      // netačno / netacno
    KwI,            // i
    KwIli,          // ili
    KwNe,           // ne

    // I/O, errors, modules
    KwIspisi,       // ispisi / ispiši
    KwUcitaj,       // ucitaj / učitaj
    KwPokusaj,      // pokusaj / pokušaj
    KwUhvati,       // uhvati
    KwBaci,         // baci
    KwKoristi,      // koristi

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %

    Equal,          // =
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=

    EqualEqual,     // ==
    BangEqual,      // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=

    // Delimiters
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    LBrace,         // {
    RBrace,         // }
    Comma,          // ,
    Colon,          // :
    Dot,            // .
    Semicolon,      // ;

    Invalid
};

struct Token {
    TokenType type = TokenType::Invalid;
    std::string lexeme;
    SourceLocation location;
    int64_t intValue = 0;
    double floatValue = 0.0;
    char32_t charValue = 0;
};

const char* tokenTypeName(TokenType type);
TokenType lookupKeyword(const std::string& ident);

} // namespace bos
