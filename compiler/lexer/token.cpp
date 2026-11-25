#include "lexer/token.hpp"

#include <unordered_map>

namespace bos {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::EndOfFile: return "EOF";
        case TokenType::Newline: return "NEWLINE";
        case TokenType::Identifier: return "IDENT";
        case TokenType::IntLiteral: return "CIJELI_LIT";
        case TokenType::FloatLiteral: return "DECIMALNI_LIT";
        case TokenType::StringLiteral: return "TEKST_LIT";
        case TokenType::CharLiteral: return "ZNAK_LIT";
        case TokenType::KwNeka: return "KW_NEKA";
        case TokenType::KwKao: return "KW_KAO";
        case TokenType::KwStalno: return "KW_STALNO";
        case TokenType::KwFunkcija: return "KW_FUNKCIJA";
        case TokenType::KwVraca: return "KW_VRACA";
        case TokenType::KwVrati: return "KW_VRATI";
        case TokenType::KwZapis: return "KW_ZAPIS";
        case TokenType::KwKlasa: return "KW_KLASA";
        case TokenType::KwNasljeduje: return "KW_NASLJEDUJE";
        case TokenType::KwJavno: return "KW_JAVNO";
        case TokenType::KwPrivatno: return "KW_PRIVATNO";
        case TokenType::KwKonstruktor: return "KW_KONSTRUKTOR";
        case TokenType::KwOvaj: return "KW_OVAJ";
        case TokenType::KwCijeli: return "KW_CIJELI";
        case TokenType::KwDecimalni: return "KW_DECIMALNI";
        case TokenType::KwTekst: return "KW_TEKST";
        case TokenType::KwZnak: return "KW_ZNAK";
        case TokenType::KwLogicki: return "KW_LOGICKI";
        case TokenType::KwPrazno: return "KW_PRAZNO";
        case TokenType::KwNiz: return "KW_NIZ";
        case TokenType::KwLista: return "KW_LISTA";
        case TokenType::KwRjecnik: return "KW_RJECNIK";
        case TokenType::KwAko: return "KW_AKO";
        case TokenType::KwOnda: return "KW_ONDA";
        case TokenType::KwInace: return "KW_INACE";
        case TokenType::KwKraj: return "KW_KRAJ";
        case TokenType::KwPonavljaj: return "KW_PONAVLJAJ";
        case TokenType::KwOd: return "KW_OD";
        case TokenType::KwDo: return "KW_DO";
        case TokenType::KwZa: return "KW_ZA";
        case TokenType::KwSvaki: return "KW_SVAKI";
        case TokenType::KwU: return "KW_U";
        case TokenType::KwDok: return "KW_DOK";
        case TokenType::KwRadi: return "KW_RADI";
        case TokenType::KwPrekini: return "KW_PREKINI";
        case TokenType::KwNastavi: return "KW_NASTAVI";
        case TokenType::KwTacno: return "KW_TACNO";
        case TokenType::KwNetacno: return "KW_NETACNO";
        case TokenType::KwI: return "KW_I";
        case TokenType::KwIli: return "KW_ILI";
        case TokenType::KwNe: return "KW_NE";
        case TokenType::KwIspisi: return "KW_ISPISI";
        case TokenType::KwUcitaj: return "KW_UCITAJ";
        case TokenType::KwPokusaj: return "KW_POKUSAJ";
        case TokenType::KwUhvati: return "KW_UHVATI";
        case TokenType::KwBaci: return "KW_BACI";
        case TokenType::KwKoristi: return "KW_KORISTI";
        case TokenType::Plus: return "PLUS";
        case TokenType::Minus: return "MINUS";
        case TokenType::Star: return "STAR";
        case TokenType::Slash: return "SLASH";
        case TokenType::Percent: return "PERCENT";
        case TokenType::Equal: return "EQUAL";
        case TokenType::PlusEqual: return "PLUS_EQUAL";
        case TokenType::MinusEqual: return "MINUS_EQUAL";
        case TokenType::StarEqual: return "STAR_EQUAL";
        case TokenType::SlashEqual: return "SLASH_EQUAL";
        case TokenType::EqualEqual: return "EQUAL_EQUAL";
        case TokenType::BangEqual: return "BANG_EQUAL";
        case TokenType::Less: return "LESS";
        case TokenType::LessEqual: return "LESS_EQUAL";
        case TokenType::Greater: return "GREATER";
        case TokenType::GreaterEqual: return "GREATER_EQUAL";
        case TokenType::LParen: return "LPAREN";
        case TokenType::RParen: return "RPAREN";
        case TokenType::LBracket: return "LBRACKET";
        case TokenType::RBracket: return "RBRACKET";
        case TokenType::LBrace: return "LBRACE";
        case TokenType::RBrace: return "RBRACE";
        case TokenType::Comma: return "COMMA";
        case TokenType::Colon: return "COLON";
        case TokenType::Dot: return "DOT";
        case TokenType::Semicolon: return "SEMICOLON";
        case TokenType::Invalid: return "INVALID";
    }
    return "UNKNOWN";
}

TokenType lookupKeyword(const std::string& ident) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"neka", TokenType::KwNeka},
        {"kao", TokenType::KwKao},
        {"stalno", TokenType::KwStalno},
        {"funkcija", TokenType::KwFunkcija},
        {"vraća", TokenType::KwVraca},
        {"vraca", TokenType::KwVraca},
        {"vrati", TokenType::KwVrati},
        {"zapis", TokenType::KwZapis},
        {"klasa", TokenType::KwKlasa},
        {"nasljeduje", TokenType::KwNasljeduje},
        {"nasljeđuje", TokenType::KwNasljeduje},
        {"javno", TokenType::KwJavno},
        {"privatno", TokenType::KwPrivatno},
        {"konstruktor", TokenType::KwKonstruktor},
        {"ovaj", TokenType::KwOvaj},

        {"cijeli", TokenType::KwCijeli},
        {"decimalni", TokenType::KwDecimalni},
        {"tekst", TokenType::KwTekst},
        {"znak", TokenType::KwZnak},
        {"logički", TokenType::KwLogicki},
        {"logicki", TokenType::KwLogicki},
        {"prazno", TokenType::KwPrazno},
        {"niz", TokenType::KwNiz},
        {"lista", TokenType::KwLista},
        {"rjecnik", TokenType::KwRjecnik},
        {"rječnik", TokenType::KwRjecnik},

        {"ako", TokenType::KwAko},
        {"onda", TokenType::KwOnda},
        {"inače", TokenType::KwInace},
        {"inace", TokenType::KwInace},
        {"kraj", TokenType::KwKraj},
        {"ponavljaj", TokenType::KwPonavljaj},
        {"od", TokenType::KwOd},
        {"do", TokenType::KwDo},
        {"za", TokenType::KwZa},
        {"svaki", TokenType::KwSvaki},
        {"u", TokenType::KwU},
        {"dok", TokenType::KwDok},
        {"radi", TokenType::KwRadi},
        {"prekini", TokenType::KwPrekini},
        {"nastavi", TokenType::KwNastavi},

        {"tačno", TokenType::KwTacno},
        {"tacno", TokenType::KwTacno},
        {"netačno", TokenType::KwNetacno},
        {"netacno", TokenType::KwNetacno},
        {"i", TokenType::KwI},
        {"ili", TokenType::KwIli},
        {"ne", TokenType::KwNe},

        {"ispisi", TokenType::KwIspisi},
        {"ispiši", TokenType::KwIspisi},
        {"ucitaj", TokenType::KwUcitaj},
        {"učitaj", TokenType::KwUcitaj},
        {"pokusaj", TokenType::KwPokusaj},
        {"pokušaj", TokenType::KwPokusaj},
        {"uhvati", TokenType::KwUhvati},
        {"baci", TokenType::KwBaci},
        {"koristi", TokenType::KwKoristi},
    };

    auto it = keywords.find(ident);
    if (it != keywords.end()) return it->second;
    return TokenType::Identifier;
}

} // namespace bos
