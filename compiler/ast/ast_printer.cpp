#include "ast/ast_printer.hpp"

#include <sstream>

namespace bos {

std::string AstPrinter::print(const Program& program) {
    AstPrinter printer;
    printer.out_ += "Program(" + program.filename + ")\n";
    for (const auto& stmt : program.statements) {
        if (stmt) printer.printStmt(*stmt, 1);
    }
    return printer.out_;
}

void AstPrinter::pad(int indent) {
    for (int i = 0; i < indent; ++i) out_ += "  ";
}

std::string AstPrinter::formatTypeAnn(const TypeAnnotation& ann) {
    if (ann.typeArgs.empty()) return ann.name;
    std::string s = ann.name + "<";
    for (size_t i = 0; i < ann.typeArgs.size(); ++i) {
        if (i > 0) s += ", ";
        s += formatTypeAnn(ann.typeArgs[i]);
    }
    s += ">";
    return s;
}

const char* AstPrinter::binaryOpName(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Neq: return "!=";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Lte: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Gte: return ">=";
        case BinaryOp::And: return "i";
        case BinaryOp::Or: return "ili";
    }
    return "?";
}

const char* AstPrinter::unaryOpName(UnaryOp op) {
    switch (op) {
        case UnaryOp::Negate: return "-";
        case UnaryOp::Plus: return "+";
        case UnaryOp::Not: return "ne";
    }
    return "?";
}

const char* AstPrinter::assignOpName(AssignOp op) {
    switch (op) {
        case AssignOp::Assign: return "=";
        case AssignOp::AddAssign: return "+=";
        case AssignOp::SubAssign: return "-=";
        case AssignOp::MulAssign: return "*=";
        case AssignOp::DivAssign: return "/=";
    }
    return "=";
}

void AstPrinter::printStmt(const Stmt& stmt, int indent) {
    pad(indent);
    if (auto* v = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        out_ += (v->isConst ? "StalnoDecl(" : "NekaDecl(") + v->name;
        if (v->typeAnn) out_ += " : " + formatTypeAnn(*v->typeAnn);
        else if (v->resolvedType) out_ += " : " + v->resolvedType->toString();
        out_ += ")\n";
        if (v->initializer) printExpr(*v->initializer, indent + 1);
    } else if (auto* a = dynamic_cast<const AssignStmt*>(&stmt)) {
        out_ += std::string("Assign(") + assignOpName(a->op) + ")\n";
        printExpr(*a->target, indent + 1);
        printExpr(*a->value, indent + 1);
    } else if (auto* e = dynamic_cast<const ExprStmt*>(&stmt)) {
        out_ += "ExprStmt\n";
        printExpr(*e->expr, indent + 1);
    } else if (auto* p = dynamic_cast<const PrintStmt*>(&stmt)) {
        out_ += "IspisiStmt\n";
        printExpr(*p->expr, indent + 1);
    } else if (auto* in = dynamic_cast<const InputStmt*>(&stmt)) {
        out_ += "UcitajStmt(" + in->targetName + ")\n";
    } else if (auto* iff = dynamic_cast<const IfStmt*>(&stmt)) {
        out_ += "AkoStmt\n";
        pad(indent + 1); out_ += "Uslov:\n";
        printExpr(*iff->condition, indent + 2);
        pad(indent + 1); out_ += "Onda:\n";
        for (const auto& s : iff->thenBranch) printStmt(*s, indent + 2);
        for (const auto& ei : iff->elseIfBranches) {
            pad(indent + 1); out_ += "InaceAko:\n";
            printExpr(*ei.condition, indent + 2);
            for (const auto& s : ei.body) printStmt(*s, indent + 2);
        }
        if (!iff->elseBranch.empty()) {
            pad(indent + 1); out_ += "Inace:\n";
            for (const auto& s : iff->elseBranch) printStmt(*s, indent + 2);
        }
    } else if (auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
        out_ += "DokStmt\n";
        printExpr(*w->condition, indent + 1);
        for (const auto& s : w->body) printStmt(*s, indent + 1);
    } else if (auto* fr = dynamic_cast<const ForRangeStmt*>(&stmt)) {
        out_ += "PonavljajStmt(" + fr->varName + ")\n";
        printExpr(*fr->start, indent + 1);
        printExpr(*fr->end, indent + 1);
        for (const auto& s : fr->body) printStmt(*s, indent + 1);
    } else if (auto* fe = dynamic_cast<const ForEachStmt*>(&stmt)) {
        out_ += "ZaSvakiStmt(" + fe->varName + ")\n";
        printExpr(*fe->iterable, indent + 1);
        for (const auto& s : fe->body) printStmt(*s, indent + 1);
    } else if (dynamic_cast<const BreakStmt*>(&stmt)) {
        out_ += "PrekiniStmt\n";
    } else if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        out_ += "NastaviStmt\n";
    } else if (auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
        out_ += "VratiStmt\n";
        if (r->value) printExpr(*r->value, indent + 1);
    } else if (auto* tc = dynamic_cast<const TryCatchStmt*>(&stmt)) {
        out_ += "PokusajUhvatiStmt(" + tc->catchVar + ")\n";
        pad(indent + 1); out_ += "Pokusaj:\n";
        for (const auto& s : tc->tryBody) printStmt(*s, indent + 2);
        pad(indent + 1); out_ += "Uhvati:\n";
        for (const auto& s : tc->catchBody) printStmt(*s, indent + 2);
    } else if (auto* th = dynamic_cast<const ThrowStmt*>(&stmt)) {
        out_ += "BaciStmt\n";
        printExpr(*th->expr, indent + 1);
    } else if (auto* imp = dynamic_cast<const ImportStmt*>(&stmt)) {
        out_ += "KoristiStmt(" + imp->moduleName + ")\n";
    } else if (auto* fn = dynamic_cast<const FuncDeclStmt*>(&stmt)) {
        out_ += (fn->isConstructor ? "Konstruktor(" : "FunkcijaDecl(") + fn->name + "(";
        for (size_t i = 0; i < fn->params.size(); ++i) {
            if (i > 0) out_ += ", ";
            out_ += fn->params[i].name + ": " + formatTypeAnn(fn->params[i].typeAnn);
        }
        out_ += ") -> " + formatTypeAnn(fn->returnTypeAnn) + ")\n";
        for (const auto& s : fn->body) printStmt(*s, indent + 1);
    } else if (auto* st = dynamic_cast<const StructDeclStmt*>(&stmt)) {
        out_ += "ZapisDecl(" + st->name + ")\n";
        for (const auto& f : st->fields) {
            pad(indent + 1);
            out_ += "Polje(" + f.name + ": " + formatTypeAnn(f.typeAnn) + ")\n";
        }
    } else if (auto* cl = dynamic_cast<const ClassDeclStmt*>(&stmt)) {
        out_ += "KlasaDecl(" + cl->name;
        if (!cl->baseClass.empty()) out_ += " nasljeduje " + cl->baseClass;
        out_ += ")\n";
        for (const auto& f : cl->fields) {
            pad(indent + 1);
            out_ += std::string(f.visibility == Visibility::Javno ? "javno " : "privatno ")
                 + "Polje(" + f.name + ": " + formatTypeAnn(f.typeAnn) + ")\n";
        }
        for (const auto& m : cl->methods) {
            printStmt(*m, indent + 1);
        }
    }
}

void AstPrinter::printExpr(const Expr& expr, int indent) {
    pad(indent);
    if (auto* i = dynamic_cast<const IntLiteralExpr*>(&expr)) {
        out_ += "Cijeli(" + std::to_string(i->value) + ")\n";
    } else if (auto* f = dynamic_cast<const FloatLiteralExpr*>(&expr)) {
        std::ostringstream oss; oss << f->value;
        out_ += "Decimalni(" + oss.str() + ")\n";
    } else if (auto* s = dynamic_cast<const StringLiteralExpr*>(&expr)) {
        out_ += "Tekst(\"" + s->value + "\")\n";
    } else if (auto* c = dynamic_cast<const CharLiteralExpr*>(&expr)) {
        out_ += "Znak('" + c->utf8 + "')\n";
    } else if (auto* b = dynamic_cast<const BoolLiteralExpr*>(&expr)) {
        out_ += std::string("Logicki(") + (b->value ? "tačno" : "netačno") + ")\n";
    } else if (auto* id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        out_ += "Ident(" + id->name + ")\n";
    } else if (dynamic_cast<const SelfExpr*>(&expr)) {
        out_ += "Ovaj\n";
    } else if (auto* u = dynamic_cast<const UnaryExpr*>(&expr)) {
        out_ += std::string("Unary(") + unaryOpName(u->op) + ")\n";
        printExpr(*u->operand, indent + 1);
    } else if (auto* bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        out_ += std::string("Binary(") + binaryOpName(bin->op) + ")\n";
        printExpr(*bin->left, indent + 1);
        printExpr(*bin->right, indent + 1);
    } else if (auto* call = dynamic_cast<const CallExpr*>(&expr)) {
        out_ += "Call\n";
        printExpr(*call->callee, indent + 1);
        for (const auto& arg : call->args) printExpr(*arg, indent + 1);
    } else if (auto* mem = dynamic_cast<const MemberAccessExpr*>(&expr)) {
        out_ += "Member(." + mem->member + ")\n";
        printExpr(*mem->object, indent + 1);
    } else if (auto* idx = dynamic_cast<const IndexExpr*>(&expr)) {
        out_ += "Index\n";
        printExpr(*idx->object, indent + 1);
        printExpr(*idx->index, indent + 1);
    } else if (auto* list = dynamic_cast<const ListLiteralExpr*>(&expr)) {
        out_ += "ListLiteral\n";
        for (const auto& el : list->elements) printExpr(*el, indent + 1);
    } else if (auto* dict = dynamic_cast<const DictLiteralExpr*>(&expr)) {
        out_ += "DictLiteral\n";
        for (const auto& kv : dict->entries) {
            printExpr(*kv.first, indent + 1);
            printExpr(*kv.second, indent + 1);
        }
    }
}

} // namespace bos
