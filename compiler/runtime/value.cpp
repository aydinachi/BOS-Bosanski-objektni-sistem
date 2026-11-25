#include "runtime/value.hpp"
#include "lexer/lexer.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace bos {

Value* DictObject::find(const std::string& key) {
    for (auto& kv : entries) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

const Value* DictObject::find(const std::string& key) const {
    for (const auto& kv : entries) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

void DictObject::set(const std::string& key, Value val) {
    for (auto& kv : entries) {
        if (kv.first == key) {
            kv.second = std::move(val);
            return;
        }
    }
    entries.emplace_back(key, std::move(val));
}

bool DictObject::erase(const std::string& key) {
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it->first == key) {
            entries.erase(it);
            return true;
        }
    }
    return false;
}

Value Value::prazno() {
    Value v;
    v.kind = ValueKind::Prazno;
    return v;
}

Value Value::cijeli(int64_t x) {
    Value v;
    v.kind = ValueKind::Cijeli;
    v.intVal = x;
    return v;
}

Value Value::decimalni(double x) {
    Value v;
    v.kind = ValueKind::Decimalni;
    v.floatVal = x;
    return v;
}

Value Value::logicki(bool x) {
    Value v;
    v.kind = ValueKind::Logicki;
    v.boolVal = x;
    return v;
}

Value Value::znak(char32_t cp) {
    Value v;
    v.kind = ValueKind::Znak;
    v.charVal = cp;
    return v;
}

Value Value::tekst(std::string s) {
    Value v;
    v.kind = ValueKind::Tekst;
    v.strVal = std::move(s);
    return v;
}

Value Value::niz(std::vector<Value> elems) {
    Value v;
    v.kind = ValueKind::Niz;
    v.listVal = std::make_shared<ListObject>();
    v.listVal->elements = std::move(elems);
    return v;
}

Value Value::lista(std::vector<Value> elems) {
    Value v;
    v.kind = ValueKind::Lista;
    v.listVal = std::make_shared<ListObject>();
    v.listVal->elements = std::move(elems);
    return v;
}

Value Value::rjecnik() {
    Value v;
    v.kind = ValueKind::Rjecnik;
    v.dictVal = std::make_shared<DictObject>();
    return v;
}

Value Value::objekt(std::string typeName, bool isClass) {
    Value v;
    v.kind = ValueKind::Objekt;
    v.objVal = std::make_shared<ObjectInstance>();
    v.objVal->typeName = std::move(typeName);
    v.objVal->isClass = isClass;
    return v;
}

Value Value::greska(std::string message, int64_t code) {
    Value v;
    v.kind = ValueKind::Greska;
    v.errVal = std::make_shared<ErrorObject>();
    v.errVal->message = std::move(message);
    v.errVal->code = code;
    return v;
}

Value Value::datoteka(std::shared_ptr<FileObject> f) {
    Value v;
    v.kind = ValueKind::Datoteka;
    v.fileVal = std::move(f);
    return v;
}

Value Value::modul(std::string modName) {
    Value v;
    v.kind = ValueKind::Modul;
    v.strVal = std::move(modName);
    return v;
}

std::string Value::toString() const {
    switch (kind) {
        case ValueKind::Prazno: return "prazno";
        case ValueKind::Cijeli: return std::to_string(intVal);
        case ValueKind::Decimalni: {
            std::ostringstream oss;
            oss << std::setprecision(15) << floatVal;
            return oss.str();
        }
        case ValueKind::Logicki: return boolVal ? "tačno" : "netačno";
        case ValueKind::Znak: return Lexer::encodeUtf8(charVal);
        case ValueKind::Tekst: return strVal;
        case ValueKind::Niz:
        case ValueKind::Lista: {
            std::string out = "[";
            if (listVal) {
                for (size_t i = 0; i < listVal->elements.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += listVal->elements[i].toString();
                }
            }
            out += "]";
            return out;
        }
        case ValueKind::Rjecnik: {
            std::string out = "{";
            if (dictVal) {
                for (size_t i = 0; i < dictVal->entries.size(); ++i) {
                    if (i > 0) out += ", ";
                    out += dictVal->entries[i].first + ": " + dictVal->entries[i].second.toString();
                }
            }
            out += "}";
            return out;
        }
        case ValueKind::Objekt:
            return objVal ? ("<" + objVal->typeName + ">") : "<objekt>";
        case ValueKind::Greska:
            return errVal ? ("Greska(" + errVal->message + ")") : "Greska";
        case ValueKind::Datoteka:
            return fileVal ? ("Datoteka(" + fileVal->path + ")") : "Datoteka";
        case ValueKind::Modul:
            return "modul(" + strVal + ")";
    }
    return "";
}

bool Value::equals(const Value& other) const {
    if (isNumeric() && other.isNumeric()) {
        return asNumber() == other.asNumber();
    }
    if (kind != other.kind) return false;
    switch (kind) {
        case ValueKind::Prazno: return true;
        case ValueKind::Logicki: return boolVal == other.boolVal;
        case ValueKind::Znak: return charVal == other.charVal;
        case ValueKind::Tekst: return strVal == other.strVal;
        case ValueKind::Niz:
        case ValueKind::Lista:
            if (!listVal || !other.listVal) return listVal == other.listVal;
            if (listVal->elements.size() != other.listVal->elements.size()) return false;
            for (size_t i = 0; i < listVal->elements.size(); ++i) {
                if (!listVal->elements[i].equals(other.listVal->elements[i])) return false;
            }
            return true;
        case ValueKind::Objekt: return objVal == other.objVal;
        case ValueKind::Greska: return errVal == other.errVal;
        case ValueKind::Datoteka: return fileVal == other.fileVal;
        case ValueKind::Modul: return strVal == other.strVal;
        default: return false;
    }
}

} // namespace bos
