#pragma once

#include <memory>
#include <string>
#include <vector>

namespace bos {

enum class TypeKind {
    Cijeli,
    Decimalni,
    Tekst,
    Znak,
    Logicki,
    Prazno,
    Niz,
    Lista,
    Rjecnik,
    Zapis,
    Klasa,
    Greska,
    Datoteka,
    Modul,
    Any,
    Error
};

struct Type;
using TypePtr = std::shared_ptr<Type>;

struct Type {
    TypeKind kind = TypeKind::Error;
    std::string name;                  // For Zapis, Klasa, Modul
    std::vector<TypePtr> typeArgs;     // For Niz<T>, Lista<T>, Rjecnik<K, V>

    bool isNumeric() const {
        return kind == TypeKind::Cijeli || kind == TypeKind::Decimalni;
    }

    bool isError() const { return kind == TypeKind::Error; }
    bool isAny() const { return kind == TypeKind::Any; }

    std::string toString() const;
    bool equals(const Type& other) const;

    static TypePtr cijeli();
    static TypePtr decimalni();
    static TypePtr tekst();
    static TypePtr znak();
    static TypePtr logicki();
    static TypePtr prazno();
    static TypePtr greska();
    static TypePtr datoteka();
    static TypePtr any();
    static TypePtr error();
    static TypePtr niz(TypePtr elem);
    static TypePtr lista(TypePtr elem);
    static TypePtr rjecnik(TypePtr key, TypePtr val);
    static TypePtr zapis(const std::string& name);
    static TypePtr klasa(const std::string& name);
    static TypePtr modul(const std::string& name);
};

} // namespace bos
