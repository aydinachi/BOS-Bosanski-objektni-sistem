#include "runtime/stdlib.hpp"
#include "lexer/lexer.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace bos {

StandardLibrary::StandardLibrary(std::string sandboxDir)
    : sandboxDir_(std::move(sandboxDir)) {}

int64_t StandardLibrary::utf8Length(const std::string& s) {
    int64_t count = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        char32_t cp = 0;
        size_t len = 1;
        Lexer::decodeUtf8(s, pos, cp, len);
        pos += len;
        count++;
    }
    return count;
}

char32_t StandardLibrary::utf8CharAt(const std::string& s, int64_t index) {
    if (index < 0) {
        throw BosRuntimeException("Indeks teksta ne može biti negativan: " + std::to_string(index) + ".", 103);
    }
    int64_t cur = 0;
    size_t pos = 0;
    while (pos < s.size()) {
        char32_t cp = 0;
        size_t len = 1;
        Lexer::decodeUtf8(s, pos, cp, len);
        if (cur == index) return cp;
        pos += len;
        cur++;
    }
    throw BosRuntimeException("Indeks teksta " + std::to_string(index) + " je izvan opsega (dužina " + std::to_string(cur) + ").", 103);
}

std::string StandardLibrary::utf8ToUpper(const std::string& s) {
    std::string out;
    size_t pos = 0;
    while (pos < s.size()) {
        char32_t cp = 0;
        size_t len = 1;
        Lexer::decodeUtf8(s, pos, cp, len);
        pos += len;

        if (cp >= 'a' && cp <= 'z') cp = cp - 'a' + 'A';
        else if (cp == 0x010D) cp = 0x010C; // č -> Č
        else if (cp == 0x0107) cp = 0x0106; // ć -> Ć
        else if (cp == 0x017E) cp = 0x017D; // ž -> Ž
        else if (cp == 0x0161) cp = 0x0160; // š -> Š
        else if (cp == 0x0111) cp = 0x0110; // đ -> Đ
        out += Lexer::encodeUtf8(cp);
    }
    return out;
}

std::string StandardLibrary::utf8ToLower(const std::string& s) {
    std::string out;
    size_t pos = 0;
    while (pos < s.size()) {
        char32_t cp = 0;
        size_t len = 1;
        Lexer::decodeUtf8(s, pos, cp, len);
        pos += len;

        if (cp >= 'A' && cp <= 'Z') cp = cp - 'A' + 'a';
        else if (cp == 0x010C) cp = 0x010D; // Č -> č
        else if (cp == 0x0106) cp = 0x0107; // Ć -> ć
        else if (cp == 0x017D) cp = 0x017E; // Ž -> ž
        else if (cp == 0x0160) cp = 0x0161; // Š -> š
        else if (cp == 0x0110) cp = 0x0111; // Đ -> đ
        out += Lexer::encodeUtf8(cp);
    }
    return out;
}

std::string StandardLibrary::resolveSafePath(const std::string& userPath) const {
    if (sandboxDir_.empty()) {
        return userPath;
    }
    fs::path base = fs::weakly_canonical(fs::path(sandboxDir_));
    fs::create_directories(base);
    fs::path candidate = fs::weakly_canonical(base / fs::path(userPath));
    std::string baseStr = base.string();
    std::string candStr = candidate.string();
    if (baseStr.empty() || baseStr.back() != fs::path::preferred_separator) {
        baseStr.push_back(fs::path::preferred_separator);
    }
    if (candStr.rfind(baseStr, 0) != 0) {
        throw BosRuntimeException("Sigurnosna greška: pristup datoteci izvan radnog direktorija nije dozvoljen ('" + userPath + "').", 403);
    }
    return candStr;
}

Value StandardLibrary::getModuleConstant(const std::string& moduleName,
                                         const std::string& constName) {
    if (moduleName == "matematika") {
        if (constName == "PI") return Value::decimalni(3.14159265358979323846);
        if (constName == "E") return Value::decimalni(2.71828182845904523536);
    }
    throw BosRuntimeException("Nepoznata konstanta '" + moduleName + "." + constName + "'.", 104);
}

Value StandardLibrary::callModuleFunction(const std::string& moduleName,
                                          const std::string& funcName,
                                          const std::vector<Value>& args) {
    if (moduleName == "matematika") {
        if (funcName == "apsolutno") {
            if (args.size() != 1) throw BosRuntimeException("matematika.apsolutno očekuje 1 argument.", 105);
            if (args[0].kind == ValueKind::Cijeli) return Value::cijeli(std::llabs(args[0].intVal));
            return Value::decimalni(std::fabs(args[0].asNumber()));
        }
        if (funcName == "korijen") {
            if (args.size() != 1) throw BosRuntimeException("matematika.korijen očekuje 1 argument.", 105);
            double x = args[0].asNumber();
            if (x < 0.0) throw BosRuntimeException("Kvadratni korijen negativnog broja nije definisan.", 106);
            return Value::decimalni(std::sqrt(x));
        }
        if (funcName == "stepen") {
            if (args.size() != 2) throw BosRuntimeException("matematika.stepen očekuje 2 argumenta.", 105);
            return Value::decimalni(std::pow(args[0].asNumber(), args[1].asNumber()));
        }
        if (funcName == "minimum") {
            if (args.size() != 2) throw BosRuntimeException("matematika.minimum očekuje 2 argumenta.", 105);
            if (args[0].kind == ValueKind::Cijeli && args[1].kind == ValueKind::Cijeli) {
                return Value::cijeli(std::min(args[0].intVal, args[1].intVal));
            }
            return Value::decimalni(std::min(args[0].asNumber(), args[1].asNumber()));
        }
        if (funcName == "maksimum") {
            if (args.size() != 2) throw BosRuntimeException("matematika.maksimum očekuje 2 argumenta.", 105);
            if (args[0].kind == ValueKind::Cijeli && args[1].kind == ValueKind::Cijeli) {
                return Value::cijeli(std::max(args[0].intVal, args[1].intVal));
            }
            return Value::decimalni(std::max(args[0].asNumber(), args[1].asNumber()));
        }
        throw BosRuntimeException("Nepoznata funkcija 'matematika." + funcName + "'.", 104);
    }

    if (moduleName == "tekst") {
        if (funcName == "duzina") {
            if (args.size() != 1) throw BosRuntimeException("tekst.duzina očekuje 1 argument.", 105);
            return Value::cijeli(utf8Length(args[0].strVal));
        }
        if (funcName == "veliko") {
            if (args.size() != 1) throw BosRuntimeException("tekst.veliko očekuje 1 argument.", 105);
            return Value::tekst(utf8ToUpper(args[0].strVal));
        }
        if (funcName == "malo") {
            if (args.size() != 1) throw BosRuntimeException("tekst.malo očekuje 1 argument.", 105);
            return Value::tekst(utf8ToLower(args[0].strVal));
        }
        if (funcName == "sadrzi") {
            if (args.size() != 2) throw BosRuntimeException("tekst.sadrzi očekuje 2 argumenta.", 105);
            return Value::logicki(args[0].strVal.find(args[1].strVal) != std::string::npos);
        }
        throw BosRuntimeException("Nepoznata funkcija 'tekst." + funcName + "'.", 104);
    }

    if (moduleName == "kolekcije") {
        if (funcName == "lista") {
            return Value::lista(args);
        }
        if (funcName == "niz") {
            return Value::niz(args);
        }
        if (funcName == "dodaj") {
            if (args.size() != 2 || !args[0].listVal) {
                throw BosRuntimeException("kolekcije.dodaj očekuje listu/niz i element.", 105);
            }
            args[0].listVal->elements.push_back(args[1]);
            return Value::prazno();
        }
        if (funcName == "ukloni") {
            if (args.size() != 2 || !args[0].listVal) {
                throw BosRuntimeException("kolekcije.ukloni očekuje listu/niz i indeks.", 105);
            }
            int64_t idx = args[1].intVal;
            auto& vec = args[0].listVal->elements;
            if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                throw BosRuntimeException("Indeks " + std::to_string(idx) + " je izvan opsega kolekcije.", 103);
            }
            vec.erase(vec.begin() + idx);
            return Value::prazno();
        }
        if (funcName == "velicina") {
            if (args.size() != 1) throw BosRuntimeException("kolekcije.velicina očekuje 1 argument.", 105);
            if (args[0].listVal) return Value::cijeli(static_cast<int64_t>(args[0].listVal->elements.size()));
            if (args[0].dictVal) return Value::cijeli(static_cast<int64_t>(args[0].dictVal->entries.size()));
            if (args[0].kind == ValueKind::Tekst) return Value::cijeli(utf8Length(args[0].strVal));
            return Value::cijeli(0);
        }
        throw BosRuntimeException("Nepoznata funkcija 'kolekcije." + funcName + "'.", 104);
    }

    if (moduleName == "datoteke") {
        if (funcName == "otvori") {
            if (args.empty() || args.size() > 2) {
                throw BosRuntimeException("datoteke.otvori očekuje putanju i opcionalni način rada.", 105);
            }
            std::string rawPath = args[0].strVal;
            std::string mode = args.size() == 2 ? args[1].strVal : "r";
            std::string resolved = resolveSafePath(rawPath);

            std::ios::openmode om = std::ios::in;
            if (mode == "r" || mode == "citanje") {
                om = std::ios::in;
            } else if (mode == "w" || mode == "pisanje") {
                om = std::ios::out | std::ios::trunc;
            } else if (mode == "a" || mode == "dodavanje") {
                om = std::ios::out | std::ios::app;
            } else if (mode == "rw" || mode == "r+" || mode == "w+") {
                om = std::ios::in | std::ios::out | std::ios::trunc;
            } else {
                throw BosRuntimeException("Nepoznat način otvaranja datoteke: '" + mode + "'.", 107);
            }

            auto fobj = std::make_shared<FileObject>();
            fobj->path = resolved;
            fobj->mode = mode;
            fobj->stream = std::make_shared<std::fstream>(resolved, om);
            if (!fobj->stream->is_open()) {
                throw BosRuntimeException("Nije moguće otvoriti datoteku '" + rawPath + "'.", 108);
            }
            fobj->isOpen = true;
            return Value::datoteka(fobj);
        }
        if (funcName == "procitaj") {
            if (args.size() != 1 || !args[0].fileVal || !args[0].fileVal->isOpen) {
                throw BosRuntimeException("Datoteka nije otvorena za čitanje.", 109);
            }
            auto& st = *args[0].fileVal->stream;
            st.clear();
            st.seekg(0, std::ios::beg);
            std::ostringstream oss;
            oss << st.rdbuf();
            return Value::tekst(oss.str());
        }
        if (funcName == "zapisi") {
            if (args.size() != 2 || !args[0].fileVal || !args[0].fileVal->isOpen) {
                throw BosRuntimeException("Datoteka nije otvorena za pisanje.", 109);
            }
            auto& st = *args[0].fileVal->stream;
            st << args[1].strVal;
            st.flush();
            if (!st.good()) {
                throw BosRuntimeException("Greška pri pisanju u datoteku '" + args[0].fileVal->path + "'.", 110);
            }
            return Value::prazno();
        }
        if (funcName == "zatvori") {
            if (args.size() != 1 || !args[0].fileVal) {
                throw BosRuntimeException("Neispravna referenca datoteke za zatvaranje.", 109);
            }
            if (args[0].fileVal->isOpen && args[0].fileVal->stream) {
                args[0].fileVal->stream->close();
                args[0].fileVal->isOpen = false;
            }
            return Value::prazno();
        }
        throw BosRuntimeException("Nepoznata funkcija 'datoteke." + funcName + "'.", 104);
    }

    throw BosRuntimeException("Nepoznat modul '" + moduleName + "'.", 104);
}

Value StandardLibrary::callBuiltinMethod(Value& receiver,
                                         const std::string& methodName,
                                         const std::vector<Value>& args) {
    if (receiver.kind == ValueKind::Lista || receiver.kind == ValueKind::Niz) {
        if (!receiver.listVal) {
            throw BosRuntimeException("Kolekcija nije inicijalizovana.", 103);
        }
        if (methodName == "dodaj") {
            if (args.size() != 1) throw BosRuntimeException("dodaj() očekuje 1 argument.", 105);
            receiver.listVal->elements.push_back(args[0]);
            return Value::prazno();
        }
        if (methodName == "ukloni") {
            if (args.size() != 1) throw BosRuntimeException("ukloni() očekuje 1 argument.", 105);
            int64_t idx = args[0].intVal;
            auto& vec = receiver.listVal->elements;
            if (idx < 0 || static_cast<size_t>(idx) >= vec.size()) {
                throw BosRuntimeException("Indeks " + std::to_string(idx) + " je izvan opsega kolekcije (veličina " + std::to_string(vec.size()) + ").", 103);
            }
            vec.erase(vec.begin() + idx);
            return Value::prazno();
        }
        if (methodName == "velicina") {
            return Value::cijeli(static_cast<int64_t>(receiver.listVal->elements.size()));
        }
    }

    if (receiver.kind == ValueKind::Rjecnik && receiver.dictVal) {
        if (methodName == "velicina") {
            return Value::cijeli(static_cast<int64_t>(receiver.dictVal->entries.size()));
        }
        if (methodName == "sadrzi" && args.size() == 1) {
            return Value::logicki(receiver.dictVal->find(args[0].toString()) != nullptr);
        }
        if (methodName == "ukloni" && args.size() == 1) {
            receiver.dictVal->erase(args[0].toString());
            return Value::prazno();
        }
    }

    if (receiver.kind == ValueKind::Tekst) {
        if (methodName == "duzina") return Value::cijeli(utf8Length(receiver.strVal));
        if (methodName == "veliko") return Value::tekst(utf8ToUpper(receiver.strVal));
        if (methodName == "malo") return Value::tekst(utf8ToLower(receiver.strVal));
        if (methodName == "sadrzi" && args.size() == 1) {
            return Value::logicki(receiver.strVal.find(args[0].strVal) != std::string::npos);
        }
    }

    throw BosRuntimeException("Nepoznata ugrađena metoda '" + methodName + "'.", 104);
}

} // namespace bos
