#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace bos {

enum class ValueKind {
    Prazno,
    Cijeli,
    Decimalni,
    Logicki,
    Znak,
    Tekst,
    Niz,
    Lista,
    Rjecnik,
    Objekt,
    Greska,
    Datoteka,
    Modul
};

struct Value;

struct ListObject {
    std::vector<Value> elements;
};

struct DictObject {
    std::vector<std::pair<std::string, Value>> entries; // preserves insertion order
    Value* find(const std::string& key);
    const Value* find(const std::string& key) const;
    void set(const std::string& key, Value val);
    bool erase(const std::string& key);
};

struct ObjectInstance {
    std::string typeName;
    bool isClass = false;
    std::unordered_map<std::string, Value> fields;
};

struct ErrorObject {
    std::string message;
    int64_t code = 1;
};

struct FileObject {
    std::string path;
    std::string mode;
    std::shared_ptr<std::fstream> stream;
    bool isOpen = false;
};

struct Value {
    ValueKind kind = ValueKind::Prazno;
    int64_t intVal = 0;
    double floatVal = 0.0;
    bool boolVal = false;
    char32_t charVal = 0;
    std::string strVal;
    std::shared_ptr<ListObject> listVal;
    std::shared_ptr<DictObject> dictVal;
    std::shared_ptr<ObjectInstance> objVal;
    std::shared_ptr<ErrorObject> errVal;
    std::shared_ptr<FileObject> fileVal;

    static Value prazno();
    static Value cijeli(int64_t v);
    static Value decimalni(double v);
    static Value logicki(bool v);
    static Value znak(char32_t cp);
    static Value tekst(std::string s);
    static Value niz(std::vector<Value> elems = {});
    static Value lista(std::vector<Value> elems = {});
    static Value rjecnik();
    static Value objekt(std::string typeName, bool isClass);
    static Value greska(std::string message, int64_t code = 1);
    static Value datoteka(std::shared_ptr<FileObject> f);
    static Value modul(std::string modName);

    bool isNumeric() const {
        return kind == ValueKind::Cijeli || kind == ValueKind::Decimalni;
    }
    double asNumber() const {
        return kind == ValueKind::Cijeli ? static_cast<double>(intVal) : floatVal;
    }

    std::string toString() const;
    bool equals(const Value& other) const;
};

} // namespace bos
