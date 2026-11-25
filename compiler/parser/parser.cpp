#include "parser/parser.hpp"

#include <algorithm>

namespace bos {

Parser::Parser(std::vector<Token> tokens, std::string filename, DiagnosticEngine& diagnostics)
    : tokens_(std::move(tokens)), filename_(std::move(filename)), diagnostics_(diagnostics) {}

const Token& Parser::peek(size_t offset) const {
    size_t idx = current_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

const Token& Parser::previous() const {
    return tokens_[current_ > 0 ? current_ - 1 : 0];
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::EndOfFile;
}

const Token& Parser::advance() {
    if (!isAtEnd()) current_++;
    return previous();
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

void Parser::skipNewlines() {
    while (check(TokenType::Newline) || check(TokenType::Semicolon)) {
        advance();
    }
}

bool Parser::consumeEndOfStatement() {
    if (check(TokenType::Newline) || check(TokenType::Semicolon)) {
        skipNewlines();
        return true;
    }
    if (check(TokenType::EndOfFile) || check(TokenType::KwKraj) ||
        check(TokenType::KwInace) || check(TokenType::KwUhvati)) {
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message, const std::string& code) {
    if (check(type)) return advance();
    diagnostics_.error(DiagnosticStage::Parser, peek().location, message, code);
    return peek();
}

bool Parser::isIdentifierToken(TokenType type) const {
    // Allow `i` (also logical AND in infix position) as a variable/parameter identifier.
    return type == TokenType::Identifier || type == TokenType::KwI;
}

bool Parser::isMemberNameToken(TokenType type) const {
    if (isIdentifierToken(type)) return true;
    switch (type) {
        case TokenType::KwTekst:
        case TokenType::KwLista:
        case TokenType::KwNiz:
        case TokenType::KwRjecnik:
        case TokenType::KwCijeli:
        case TokenType::KwDecimalni:
        case TokenType::KwZnak:
        case TokenType::KwLogicki:
        case TokenType::KwU:
        case TokenType::KwOd:
        case TokenType::KwDo:
            return true;
        default:
            return false;
    }
}

std::string Parser::expectIdentifier(const std::string& contextMessage, const std::string& code) {
    if (isIdentifierToken(peek().type)) {
        return advance().lexeme;
    }
    diagnostics_.error(DiagnosticStage::Parser, peek().location, contextMessage, code);
    return "<greska>";
}

void Parser::synchronize() {
    while (!isAtEnd()) {
        if (peek().type == TokenType::Newline || peek().type == TokenType::Semicolon) {
            skipNewlines();
            return;
        }
        switch (peek().type) {
            case TokenType::KwNeka:
            case TokenType::KwStalno:
            case TokenType::KwFunkcija:
            case TokenType::KwZapis:
            case TokenType::KwKlasa:
            case TokenType::KwAko:
            case TokenType::KwDok:
            case TokenType::KwPonavljaj:
            case TokenType::KwZa:
            case TokenType::KwIspisi:
            case TokenType::KwUcitaj:
            case TokenType::KwVrati:
            case TokenType::KwPokusaj:
            case TokenType::KwKoristi:
            case TokenType::KwKraj:
            case TokenType::KwInace:
            case TokenType::KwUhvati:
                return;
            default:
                advance();
                break;
        }
    }
}

Program Parser::parseProgram() {
    Program program;
    program.filename = filename_;
    skipNewlines();

    while (!isAtEnd()) {
        size_t before = current_;
        auto stmt = parseStatement();
        if (stmt) {
            program.statements.push_back(std::move(stmt));
        } else {
            synchronize();
            if (current_ == before && !isAtEnd()) {
                advance();
            }
        }
        skipNewlines();
    }
    return program;
}

TypeAnnotation Parser::parseTypeAnnotation() {
    TypeAnnotation ann;
    ann.location = peek().location;

    TokenType t = peek().type;
    switch (t) {
        case TokenType::KwCijeli:
        case TokenType::KwDecimalni:
        case TokenType::KwTekst:
        case TokenType::KwZnak:
        case TokenType::KwLogicki:
        case TokenType::KwPrazno:
        case TokenType::KwNiz:
        case TokenType::KwLista:
        case TokenType::KwRjecnik:
        case TokenType::Identifier:
            ann.name = advance().lexeme;
            // Normalize Bosnian diacritic variants of built-in types
            if (t == TokenType::KwLogicki) ann.name = "logički";
            if (t == TokenType::KwRjecnik) ann.name = "rjecnik";
            break;
        default:
            diagnostics_.error(DiagnosticStage::Parser, peek().location,
                               "Očekivan je naziv tipa (npr. cijeli, tekst, niz<cijeli>), ali je pronađeno '" + peek().lexeme + "'.",
                               "P001");
            ann.name = "<greska>";
            return ann;
    }

    if (match(TokenType::Less)) {
        do {
            ann.typeArgs.push_back(parseTypeAnnotation());
        } while (match(TokenType::Comma));
        expect(TokenType::Greater, "Očekivan znak '>' za zatvaranje generičkog tipa.", "P002");
    }

    return ann;
}

std::vector<StmtPtr> Parser::parseBlockUntil(const std::vector<TokenType>& stopTokens) {
    std::vector<StmtPtr> stmts;
    skipNewlines();
    while (!isAtEnd()) {
        if (std::find(stopTokens.begin(), stopTokens.end(), peek().type) != stopTokens.end()) {
            break;
        }
        size_t before = current_;
        auto stmt = parseStatement();
        if (stmt) {
            stmts.push_back(std::move(stmt));
        } else {
            synchronize();
            if (current_ == before && !isAtEnd()) {
                advance();
            }
        }
        skipNewlines();
    }
    return stmts;
}

StmtPtr Parser::parseStatement() {
    skipNewlines();
    switch (peek().type) {
        case TokenType::KwNeka:
            return parseVarDecl(false);
        case TokenType::KwStalno:
            return parseVarDecl(true);
        case TokenType::KwIspisi:
            return parsePrintStmt();
        case TokenType::KwUcitaj:
            return parseInputStmt();
        case TokenType::KwAko:
            return parseIfStmt();
        case TokenType::KwDok:
            return parseWhileStmt();
        case TokenType::KwPonavljaj:
            return parseForRangeStmt();
        case TokenType::KwZa:
            return parseForEachStmt();
        case TokenType::KwPrekini: {
            auto s = std::make_unique<BreakStmt>();
            s->location = advance().location;
            consumeEndOfStatement();
            return s;
        }
        case TokenType::KwNastavi: {
            auto s = std::make_unique<ContinueStmt>();
            s->location = advance().location;
            consumeEndOfStatement();
            return s;
        }
        case TokenType::KwVrati:
            return parseReturnStmt();
        case TokenType::KwPokusaj:
            return parseTryCatchStmt();
        case TokenType::KwBaci:
            return parseThrowStmt();
        case TokenType::KwKoristi:
            return parseImportStmt();
        case TokenType::KwFunkcija:
            return parseFuncDecl(Visibility::Javno, false);
        case TokenType::KwZapis:
            return parseStructDecl();
        case TokenType::KwKlasa:
            return parseClassDecl();
        default:
            return parseExprOrAssignStmt();
    }
}

StmtPtr Parser::parseVarDecl(bool isConst) {
    auto stmt = std::make_unique<VarDeclStmt>();
    stmt->location = advance().location; // neka / stalno
    stmt->isConst = isConst;
    stmt->name = expectIdentifier("Očekivan je naziv promjenljive nakon '" + std::string(isConst ? "stalno" : "neka") + "'.", "P003");

    if (match(TokenType::KwKao)) {
        stmt->typeAnn = parseTypeAnnotation();
    }

    if (match(TokenType::Equal)) {
        stmt->initializer = parseExpression();
    } else if (!stmt->typeAnn.has_value()) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Deklaracija promjenljive '" + stmt->name + "' mora imati ili eksplicitni tip ('kao ...') ili početnu vrijednost ('= ...').",
                           "P004");
    } else if (isConst) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Konstanta '" + stmt->name + "' mora imati početnu vrijednost.", "P005");
    }

    if (!consumeEndOfStatement()) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Neočekivan token '" + peek().lexeme + "' na kraju deklaracije promjenljive.", "P006");
    }
    return stmt;
}

StmtPtr Parser::parsePrintStmt() {
    auto stmt = std::make_unique<PrintStmt>();
    stmt->location = advance().location; // ispisi
    stmt->expr = parseExpression();
    if (!consumeEndOfStatement()) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Neočekivan token '" + peek().lexeme + "' nakon naredbe 'ispisi'.", "P007");
    }
    return stmt;
}

StmtPtr Parser::parseInputStmt() {
    auto stmt = std::make_unique<InputStmt>();
    stmt->location = advance().location; // ucitaj
    stmt->targetName = expectIdentifier("Očekivan je naziv promjenljive nakon naredbe 'ucitaj'.", "P008");
    if (!consumeEndOfStatement()) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Neočekivan token '" + peek().lexeme + "' nakon naredbe 'ucitaj'.", "P009");
    }
    return stmt;
}

StmtPtr Parser::parseIfStmt() {
    auto stmt = std::make_unique<IfStmt>();
    stmt->location = advance().location; // ako
    stmt->condition = parseExpression();
    expect(TokenType::KwOnda, "Očekivana ključna riječ 'onda' nakon uslova u naredbi 'ako'.", "P010");
    skipNewlines();

    stmt->thenBranch = parseBlockUntil({TokenType::KwInace, TokenType::KwKraj});

    while (match(TokenType::KwInace)) {
        if (check(TokenType::KwAko)) {
            ElseIfBranch branch;
            branch.location = advance().location; // ako
            branch.condition = parseExpression();
            expect(TokenType::KwOnda, "Očekivana ključna riječ 'onda' nakon uslova u 'inače ako'.", "P011");
            skipNewlines();
            branch.body = parseBlockUntil({TokenType::KwInace, TokenType::KwKraj});
            stmt->elseIfBranches.push_back(std::move(branch));
        } else {
            skipNewlines();
            stmt->elseBranch = parseBlockUntil({TokenType::KwKraj});
            break;
        }
    }

    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje 'ako' bloka.", "P012");
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseWhileStmt() {
    auto stmt = std::make_unique<WhileStmt>();
    stmt->location = advance().location; // dok
    stmt->condition = parseExpression();
    expect(TokenType::KwRadi, "Očekivana ključna riječ 'radi' nakon uslova u 'dok' petlji.", "P013");
    skipNewlines();
    stmt->body = parseBlockUntil({TokenType::KwKraj});
    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje 'dok' petlje.", "P014");
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseForRangeStmt() {
    auto stmt = std::make_unique<ForRangeStmt>();
    stmt->location = advance().location; // ponavljaj
    stmt->varName = expectIdentifier("Očekivan je naziv brojača nakon ključne riječi 'ponavljaj'.", "P015");
    expect(TokenType::KwOd, "Očekivana ključna riječ 'od' u 'ponavljaj' petlji.", "P016");
    stmt->start = parseExpression();
    expect(TokenType::KwDo, "Očekivana ključna riječ 'do' u 'ponavljaj' petlji.", "P017");
    stmt->end = parseExpression();
    skipNewlines();
    stmt->body = parseBlockUntil({TokenType::KwKraj});
    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje 'ponavljaj' petlje.", "P018");
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseForEachStmt() {
    auto stmt = std::make_unique<ForEachStmt>();
    stmt->location = advance().location; // za
    expect(TokenType::KwSvaki, "Očekivana ključna riječ 'svaki' nakon 'za'.", "P019");
    stmt->varName = expectIdentifier("Očekivan je naziv promjenljive nakon 'za svaki'.", "P020");
    expect(TokenType::KwU, "Očekivana ključna riječ 'u' u 'za svaki' petlji.", "P021");
    stmt->iterable = parseExpression();
    skipNewlines();
    stmt->body = parseBlockUntil({TokenType::KwKraj});
    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje 'za svaki' petlje.", "P022");
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseReturnStmt() {
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->location = advance().location; // vrati
    if (!check(TokenType::Newline) && !check(TokenType::Semicolon) &&
        !check(TokenType::EndOfFile) && !check(TokenType::KwKraj) &&
        !check(TokenType::KwInace) && !check(TokenType::KwUhvati)) {
        stmt->value = parseExpression();
    }
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseTryCatchStmt() {
    auto stmt = std::make_unique<TryCatchStmt>();
    stmt->location = advance().location; // pokusaj
    skipNewlines();
    stmt->tryBody = parseBlockUntil({TokenType::KwUhvati, TokenType::KwKraj});
    expect(TokenType::KwUhvati, "Očekivana ključna riječ 'uhvati' nakon 'pokusaj' bloka.", "P023");
    stmt->catchVar = expectIdentifier("Očekivan je naziv promjenljive za grešku nakon 'uhvati'.", "P024");
    skipNewlines();
    stmt->catchBody = parseBlockUntil({TokenType::KwKraj});
    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje 'pokusaj/uhvati' bloka.", "P025");
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseThrowStmt() {
    auto stmt = std::make_unique<ThrowStmt>();
    stmt->location = advance().location; // baci
    stmt->expr = parseExpression();
    consumeEndOfStatement();
    return stmt;
}

StmtPtr Parser::parseImportStmt() {
    auto stmt = std::make_unique<ImportStmt>();
    stmt->location = advance().location; // koristi
    if (isMemberNameToken(peek().type)) {
        stmt->moduleName = advance().lexeme;
    } else {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Očekivan je naziv modula nakon ključne riječi 'koristi'.", "P026");
        stmt->moduleName = "<greska>";
    }
    consumeEndOfStatement();
    return stmt;
}

std::unique_ptr<FuncDeclStmt> Parser::parseFuncDecl(Visibility vis, bool isConstructor) {
    auto fn = std::make_unique<FuncDeclStmt>();
    fn->location = advance().location; // funkcija or konstruktor
    fn->visibility = vis;
    fn->isConstructor = isConstructor;

    if (isConstructor) {
        fn->name = "konstruktor";
    } else {
        fn->name = expectIdentifier("Očekivan je naziv funkcije nakon ključne riječi 'funkcija'.", "P027");
    }

    expect(TokenType::LParen, "Očekivana otvorena zagrada '(' u deklaraciji funkcije.", "P028");
    if (!check(TokenType::RParen)) {
        do {
            ParamDecl param;
            param.location = peek().location;
            param.name = expectIdentifier("Očekivan je naziv parametra funkcije.", "P029");
            expect(TokenType::KwKao, "Očekivana ključna riječ 'kao' nakon naziva parametra '" + param.name + "'.", "P030");
            param.typeAnn = parseTypeAnnotation();
            fn->params.push_back(std::move(param));
        } while (match(TokenType::Comma));
    }
    expect(TokenType::RParen, "Očekivana zatvorena zagrada ')' nakon liste parametara.", "P031");

    if (isConstructor) {
        fn->returnTypeAnn = TypeAnnotation{"prazno", {}, fn->location};
    } else if (match(TokenType::KwVraca)) {
        fn->returnTypeAnn = parseTypeAnnotation();
    } else {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Očekivana ključna riječ 'vraća' i povratni tip u deklaraciji funkcije '" + fn->name + "'.",
                           "P032");
        fn->returnTypeAnn = TypeAnnotation{"prazno", {}, peek().location};
    }

    skipNewlines();
    fn->body = parseBlockUntil({TokenType::KwKraj});
    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje funkcije '" + fn->name + "'.", "P033");
    consumeEndOfStatement();
    return fn;
}

StmtPtr Parser::parseStructDecl() {
    auto st = std::make_unique<StructDeclStmt>();
    st->location = advance().location; // zapis
    st->name = expectIdentifier("Očekivan je naziv zapisa nakon ključne riječi 'zapis'.", "P034");
    skipNewlines();

    while (!isAtEnd() && !check(TokenType::KwKraj)) {
        FieldDecl field;
        field.location = peek().location;
        field.visibility = Visibility::Javno;
        field.name = expectIdentifier("Očekivan je naziv polja u zapisu '" + st->name + "'.", "P035");
        expect(TokenType::KwKao, "Očekivana ključna riječ 'kao' nakon polja '" + field.name + "'.", "P036");
        field.typeAnn = parseTypeAnnotation();
        if (match(TokenType::Equal)) {
            field.defaultValue = parseExpression();
        }
        st->fields.push_back(std::move(field));
        skipNewlines();
    }

    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje zapisa '" + st->name + "'.", "P037");
    consumeEndOfStatement();
    return st;
}

StmtPtr Parser::parseClassDecl() {
    auto cl = std::make_unique<ClassDeclStmt>();
    cl->location = advance().location; // klasa
    cl->name = expectIdentifier("Očekivan je naziv klase nakon ključne riječi 'klasa'.", "P038");

    if (match(TokenType::KwNasljeduje)) {
        cl->baseClass = expectIdentifier("Očekivan je naziv bazne klase nakon 'nasljeduje'.", "P039");
    }

    skipNewlines();
    Visibility currentVis = Visibility::Javno;

    while (!isAtEnd() && !check(TokenType::KwKraj)) {
        if (match(TokenType::KwJavno)) {
            expect(TokenType::Colon, "Očekivana dvotačka ':' nakon 'javno'.", "P040");
            currentVis = Visibility::Javno;
            skipNewlines();
            continue;
        }
        if (match(TokenType::KwPrivatno)) {
            expect(TokenType::Colon, "Očekivana dvotačka ':' nakon 'privatno'.", "P041");
            currentVis = Visibility::Privatno;
            skipNewlines();
            continue;
        }
        if (check(TokenType::KwFunkcija)) {
            cl->methods.push_back(parseFuncDecl(currentVis, false));
            skipNewlines();
            continue;
        }
        if (check(TokenType::KwKonstruktor)) {
            cl->methods.push_back(parseFuncDecl(currentVis, true));
            skipNewlines();
            continue;
        }

        // Field declaration: `marka kao tekst [= expr]`
        if (isIdentifierToken(peek().type)) {
            FieldDecl field;
            field.location = peek().location;
            field.visibility = currentVis;
            field.name = advance().lexeme;
            expect(TokenType::KwKao, "Očekivana ključna riječ 'kao' nakon člana '" + field.name + "' u klasi '" + cl->name + "'.", "P042");
            field.typeAnn = parseTypeAnnotation();
            if (match(TokenType::Equal)) {
                field.defaultValue = parseExpression();
            }
            cl->fields.push_back(std::move(field));
            skipNewlines();
            continue;
        }

        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Neočekivan token '" + peek().lexeme + "' unutar klase '" + cl->name + "'.", "P043");
        advance();
        skipNewlines();
    }

    expect(TokenType::KwKraj, "Očekivana ključna riječ 'kraj' za zatvaranje klase '" + cl->name + "'.", "P044");
    consumeEndOfStatement();
    return cl;
}

StmtPtr Parser::parseExprOrAssignStmt() {
    SourceLocation loc = peek().location;
    ExprPtr expr = parseExpression();
    if (!expr) return nullptr;

    AssignOp assignOp = AssignOp::Assign;
    bool isAssign = false;
    if (match(TokenType::Equal)) { assignOp = AssignOp::Assign; isAssign = true; }
    else if (match(TokenType::PlusEqual)) { assignOp = AssignOp::AddAssign; isAssign = true; }
    else if (match(TokenType::MinusEqual)) { assignOp = AssignOp::SubAssign; isAssign = true; }
    else if (match(TokenType::StarEqual)) { assignOp = AssignOp::MulAssign; isAssign = true; }
    else if (match(TokenType::SlashEqual)) { assignOp = AssignOp::DivAssign; isAssign = true; }

    if (isAssign) {
        if (!dynamic_cast<IdentifierExpr*>(expr.get()) &&
            !dynamic_cast<MemberAccessExpr*>(expr.get()) &&
            !dynamic_cast<IndexExpr*>(expr.get())) {
            diagnostics_.error(DiagnosticStage::Parser, loc,
                               "Lijeva strana dodjele mora biti promjenljiva, polje objekta ili element kolekcije.",
                               "P045");
        }
        auto stmt = std::make_unique<AssignStmt>();
        stmt->location = loc;
        stmt->target = std::move(expr);
        stmt->op = assignOp;
        stmt->value = parseExpression();
        if (!consumeEndOfStatement()) {
            diagnostics_.error(DiagnosticStage::Parser, peek().location,
                               "Neočekivan token '" + peek().lexeme + "' nakon naredbe dodjele.", "P046");
        }
        return stmt;
    }

    auto stmt = std::make_unique<ExprStmt>();
    stmt->location = loc;
    stmt->expr = std::move(expr);
    if (!consumeEndOfStatement()) {
        diagnostics_.error(DiagnosticStage::Parser, peek().location,
                           "Neočekivan token '" + peek().lexeme + "' nakon izraza.", "P047");
    }
    return stmt;
}

// ===================== Expressions =====================

ExprPtr Parser::parseExpression() {
    return parseLogicalOr();
}

ExprPtr Parser::parseLogicalOr() {
    ExprPtr left = parseLogicalAnd();
    while (match(TokenType::KwIli)) {
        SourceLocation loc = previous().location;
        ExprPtr right = parseLogicalAnd();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = loc;
        bin->op = BinaryOp::Or;
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    ExprPtr left = parseEquality();
    while (match(TokenType::KwI)) {
        SourceLocation loc = previous().location;
        ExprPtr right = parseEquality();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = loc;
        bin->op = BinaryOp::And;
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();
    while (check(TokenType::EqualEqual) || check(TokenType::BangEqual)) {
        Token opTok = advance();
        ExprPtr right = parseComparison();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = opTok.location;
        bin->op = (opTok.type == TokenType::EqualEqual) ? BinaryOp::Eq : BinaryOp::Neq;
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    ExprPtr left = parseAdditive();
    while (check(TokenType::Less) || check(TokenType::LessEqual) ||
           check(TokenType::Greater) || check(TokenType::GreaterEqual)) {
        Token opTok = advance();
        ExprPtr right = parseAdditive();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = opTok.location;
        switch (opTok.type) {
            case TokenType::Less: bin->op = BinaryOp::Lt; break;
            case TokenType::LessEqual: bin->op = BinaryOp::Lte; break;
            case TokenType::Greater: bin->op = BinaryOp::Gt; break;
            case TokenType::GreaterEqual: bin->op = BinaryOp::Gte; break;
            default: break;
        }
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseMultiplicative();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        Token opTok = advance();
        ExprPtr right = parseMultiplicative();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = opTok.location;
        bin->op = (opTok.type == TokenType::Plus) ? BinaryOp::Add : BinaryOp::Sub;
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    ExprPtr left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash) || check(TokenType::Percent)) {
        Token opTok = advance();
        ExprPtr right = parseUnary();
        auto bin = std::make_unique<BinaryExpr>();
        bin->location = opTok.location;
        if (opTok.type == TokenType::Star) bin->op = BinaryOp::Mul;
        else if (opTok.type == TokenType::Slash) bin->op = BinaryOp::Div;
        else bin->op = BinaryOp::Mod;
        bin->left = std::move(left);
        bin->right = std::move(right);
        left = std::move(bin);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::Minus) || check(TokenType::Plus) || check(TokenType::KwNe)) {
        Token opTok = advance();
        auto un = std::make_unique<UnaryExpr>();
        un->location = opTok.location;
        if (opTok.type == TokenType::Minus) un->op = UnaryOp::Negate;
        else if (opTok.type == TokenType::Plus) un->op = UnaryOp::Plus;
        else un->op = UnaryOp::Not;
        un->operand = parseUnary();
        return un;
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    if (!expr) return nullptr;

    while (true) {
        if (match(TokenType::LParen)) {
            auto call = std::make_unique<CallExpr>();
            call->location = previous().location;
            call->callee = std::move(expr);
            if (!check(TokenType::RParen)) {
                do {
                    call->args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            expect(TokenType::RParen, "Očekivana zatvorena zagrada ')' nakon argumenata poziva.", "P048");
            expr = std::move(call);
        } else if (match(TokenType::Dot)) {
            SourceLocation dotLoc = previous().location;
            std::string memberName;
            if (isMemberNameToken(peek().type)) {
                memberName = advance().lexeme;
            } else {
                diagnostics_.error(DiagnosticStage::Parser, peek().location,
                                   "Očekivan je naziv člana ili metode nakon znaka '.'.", "P049");
                memberName = "<greska>";
            }
            auto mem = std::make_unique<MemberAccessExpr>();
            mem->location = dotLoc;
            mem->object = std::move(expr);
            mem->member = memberName;
            expr = std::move(mem);
        } else if (match(TokenType::LBracket)) {
            SourceLocation brLoc = previous().location;
            ExprPtr idx = parseExpression();
            expect(TokenType::RBracket, "Očekivana zatvorena uglasta zagrada ']' nakon indeksa.", "P050");
            auto idxExpr = std::make_unique<IndexExpr>();
            idxExpr->location = brLoc;
            idxExpr->object = std::move(expr);
            idxExpr->index = std::move(idx);
            expr = std::move(idxExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    const Token& tok = peek();

    if (tok.type == TokenType::IntLiteral) {
        advance();
        auto node = std::make_unique<IntLiteralExpr>();
        node->location = tok.location;
        node->value = tok.intValue;
        return node;
    }
    if (tok.type == TokenType::FloatLiteral) {
        advance();
        auto node = std::make_unique<FloatLiteralExpr>();
        node->location = tok.location;
        node->value = tok.floatValue;
        return node;
    }
    if (tok.type == TokenType::StringLiteral) {
        advance();
        auto node = std::make_unique<StringLiteralExpr>();
        node->location = tok.location;
        node->value = tok.lexeme;
        return node;
    }
    if (tok.type == TokenType::CharLiteral) {
        advance();
        auto node = std::make_unique<CharLiteralExpr>();
        node->location = tok.location;
        node->value = tok.charValue;
        node->utf8 = tok.lexeme;
        return node;
    }
    if (tok.type == TokenType::KwTacno || tok.type == TokenType::KwNetacno) {
        advance();
        auto node = std::make_unique<BoolLiteralExpr>();
        node->location = tok.location;
        node->value = (tok.type == TokenType::KwTacno);
        return node;
    }
    if (tok.type == TokenType::KwOvaj) {
        advance();
        auto node = std::make_unique<SelfExpr>();
        node->location = tok.location;
        return node;
    }
    // Identifier, `i` in expression position, or stdlib module keyword like `tekst.duzina(...)`
    if (isIdentifierToken(tok.type) ||
        (tok.type == TokenType::KwTekst && peek(1).type == TokenType::Dot)) {
        advance();
        auto node = std::make_unique<IdentifierExpr>();
        node->location = tok.location;
        node->name = tok.lexeme;
        return node;
    }
    if (match(TokenType::LParen)) {
        ExprPtr inner = parseExpression();
        expect(TokenType::RParen, "Očekivana zatvorena zagrada ')' u izrazu.", "P051");
        return inner;
    }
    if (match(TokenType::LBracket)) {
        auto list = std::make_unique<ListLiteralExpr>();
        list->location = previous().location;
        skipNewlines();
        if (!check(TokenType::RBracket)) {
            do {
                skipNewlines();
                if (check(TokenType::RBracket)) break;
                list->elements.push_back(parseExpression());
                skipNewlines();
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RBracket, "Očekivana zatvorena uglasta zagrada ']' u literalu kolekcije.", "P052");
        return list;
    }
    if (match(TokenType::LBrace)) {
        auto dict = std::make_unique<DictLiteralExpr>();
        dict->location = previous().location;
        skipNewlines();
        if (!check(TokenType::RBrace)) {
            do {
                skipNewlines();
                if (check(TokenType::RBrace)) break;
                ExprPtr key = parseExpression();
                expect(TokenType::Colon, "Očekivana dvotačka ':' između ključa i vrijednosti u rječniku.", "P053");
                ExprPtr val = parseExpression();
                dict->entries.emplace_back(std::move(key), std::move(val));
                skipNewlines();
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RBrace, "Očekivana zatvorena vitičasta zagrada '}' u literalu rječnika.", "P054");
        return dict;
    }

    diagnostics_.error(DiagnosticStage::Parser, tok.location,
                       "Neočekivan token '" + tok.lexeme + "' na mjestu gdje je očekivan izraz.", "P055");
    if (!isAtEnd() && tok.type != TokenType::Newline && tok.type != TokenType::KwKraj) {
        advance();
    }
    auto errNode = std::make_unique<IntLiteralExpr>();
    errNode->location = tok.location;
    errNode->value = 0;
    return errNode;
}

} // namespace bos
