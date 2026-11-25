#include "types/type.hpp"

namespace bos {

std::string Type::toString() const {
    switch (kind) {
        case TypeKind::Cijeli: return "cijeli";
        case TypeKind::Decimalni: return "decimalni";
        case TypeKind::Tekst: return "tekst";
        case TypeKind::Znak: return "znak";
        case TypeKind::Logicki: return "logički";
        case TypeKind::Prazno: return "prazno";
        case TypeKind::Greska: return "Greska";
        case TypeKind::Datoteka: return "Datoteka";
        case TypeKind::Any: return "bilo_koji";
        case TypeKind::Error: return "<greška>";
        case TypeKind::Zapis: return name;
        case TypeKind::Klasa: return name;
        case TypeKind::Modul: return "modul(" + name + ")";
        case TypeKind::Niz:
            return "niz<" + (typeArgs.empty() ? "bilo_koji" : typeArgs[0]->toString()) + ">";
        case TypeKind::Lista:
            return "lista<" + (typeArgs.empty() ? "bilo_koji" : typeArgs[0]->toString()) + ">";
        case TypeKind::Rjecnik: {
            std::string k = typeArgs.size() > 0 ? typeArgs[0]->toString() : "bilo_koji";
            std::string v = typeArgs.size() > 1 ? typeArgs[1]->toString() : "bilo_koji";
            return "rjecnik<" + k + ", " + v + ">";
        }
    }
    return "<nepoznat>";
}

bool Type::equals(const Type& other) const {
    if (kind == TypeKind::Error || other.kind == TypeKind::Error) return true;
    if (kind == TypeKind::Any || other.kind == TypeKind::Any) return true;
    if (kind != other.kind) return false;
    if (kind == TypeKind::Zapis || kind == TypeKind::Klasa || kind == TypeKind::Modul) {
        return name == other.name;
    }
    if (typeArgs.size() != other.typeArgs.size()) return false;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        if (!typeArgs[i]->equals(*other.typeArgs[i])) return false;
    }
    return true;
}

TypePtr Type::cijeli() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Cijeli, "", {}});
    return t;
}
TypePtr Type::decimalni() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Decimalni, "", {}});
    return t;
}
TypePtr Type::tekst() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Tekst, "", {}});
    return t;
}
TypePtr Type::znak() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Znak, "", {}});
    return t;
}
TypePtr Type::logicki() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Logicki, "", {}});
    return t;
}
TypePtr Type::prazno() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Prazno, "", {}});
    return t;
}
TypePtr Type::greska() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Greska, "Greska", {}});
    return t;
}
TypePtr Type::datoteka() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Datoteka, "Datoteka", {}});
    return t;
}
TypePtr Type::any() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Any, "", {}});
    return t;
}
TypePtr Type::error() {
    static auto t = std::make_shared<Type>(Type{TypeKind::Error, "", {}});
    return t;
}
TypePtr Type::niz(TypePtr elem) {
    return std::make_shared<Type>(Type{TypeKind::Niz, "", {std::move(elem)}});
}
TypePtr Type::lista(TypePtr elem) {
    return std::make_shared<Type>(Type{TypeKind::Lista, "", {std::move(elem)}});
}
TypePtr Type::rjecnik(TypePtr key, TypePtr val) {
    return std::make_shared<Type>(Type{TypeKind::Rjecnik, "", {std::move(key), std::move(val)}});
}
TypePtr Type::zapis(const std::string& name) {
    return std::make_shared<Type>(Type{TypeKind::Zapis, name, {}});
}
TypePtr Type::klasa(const std::string& name) {
    return std::make_shared<Type>(Type{TypeKind::Klasa, name, {}});
}
TypePtr Type::modul(const std::string& name) {
    return std::make_shared<Type>(Type{TypeKind::Modul, name, {}});
}

} // namespace bos
