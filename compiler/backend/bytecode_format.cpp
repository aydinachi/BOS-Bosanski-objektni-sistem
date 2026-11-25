#include "backend/bytecode_format.hpp"

#include <cstring>
#include <fstream>

namespace bos {

namespace {

template <typename T>
void writePod(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool readPod(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

void writeString(std::ofstream& out, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    writePod(out, len);
    if (len > 0) out.write(s.data(), len);
}

bool readString(std::ifstream& in, std::string& s) {
    uint32_t len = 0;
    if (!readPod(in, len)) return false;
    if (len > 64 * 1024 * 1024) return false; // sanity bound
    s.resize(len);
    if (len > 0) {
        in.read(&s[0], len);
        return static_cast<bool>(in);
    }
    return true;
}

} // namespace

bool BytecodeFormat::isBytecodeFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    uint32_t magic = 0;
    if (!readPod(in, magic)) return false;
    return magic == MAGIC;
}

bool BytecodeFormat::writeToFile(const BytecodeModule& module, const std::string& path, std::string& errorOut) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        errorOut = "Nije moguće otvoriti izlaznu bajtkod datoteku '" + path + "' za pisanje.";
        return false;
    }

    uint32_t magic = MAGIC;
    uint16_t version = FORMAT_VERSION;
    writePod(out, magic);
    writePod(out, version);
    writeString(out, module.filename);

    // Globals
    uint32_t globalCount = static_cast<uint32_t>(module.globalNames.size());
    writePod(out, globalCount);
    for (const auto& g : module.globalNames) {
        writeString(out, g);
    }

    // Structs
    uint32_t structCount = static_cast<uint32_t>(module.structs.size());
    writePod(out, structCount);
    for (const auto& st : module.structs) {
        writeString(out, st.name);
        uint32_t fc = static_cast<uint32_t>(st.fields.size());
        writePod(out, fc);
        for (const auto& f : st.fields) {
            writeString(out, f.name);
            writePod(out, f.typeTag);
        }
    }

    // Classes
    uint32_t classCount = static_cast<uint32_t>(module.classes.size());
    writePod(out, classCount);
    for (const auto& cl : module.classes) {
        writeString(out, cl.name);
        writeString(out, cl.baseClass);
        writePod(out, cl.constructorFuncIndex);
        uint32_t fc = static_cast<uint32_t>(cl.fields.size());
        writePod(out, fc);
        for (const auto& f : cl.fields) {
            writeString(out, f.name);
            writePod(out, f.typeTag);
        }
        uint32_t mc = static_cast<uint32_t>(cl.methods.size());
        writePod(out, mc);
        for (const auto& kv : cl.methods) {
            writeString(out, kv.first);
            writePod(out, kv.second);
        }
    }

    // Functions
    uint32_t fnCount = static_cast<uint32_t>(module.functions.size());
    writePod(out, fnCount);
    for (const auto& fn : module.functions) {
        writeString(out, fn.name);
        writeString(out, fn.ownerClass);
        writePod(out, fn.paramCount);
        writePod(out, fn.localCount);
        uint8_t isCtor = fn.isConstructor ? 1 : 0;
        writePod(out, isCtor);

        uint32_t cc = static_cast<uint32_t>(fn.constants.size());
        writePod(out, cc);
        for (const auto& c : fn.constants) {
            uint8_t kind = static_cast<uint8_t>(c.kind);
            writePod(out, kind);
            switch (c.kind) {
                case ConstKind::Prazno: break;
                case ConstKind::Cijeli: writePod(out, c.intVal); break;
                case ConstKind::Decimalni: writePod(out, c.floatVal); break;
                case ConstKind::Logicki: {
                    uint8_t bv = c.boolVal ? 1 : 0;
                    writePod(out, bv);
                    break;
                }
                case ConstKind::Znak: {
                    uint32_t cp = static_cast<uint32_t>(c.charVal);
                    writePod(out, cp);
                    break;
                }
                case ConstKind::Tekst: writeString(out, c.strVal); break;
            }
        }

        uint32_t ic = static_cast<uint32_t>(fn.code.size());
        writePod(out, ic);
        for (const auto& ins : fn.code) {
            uint8_t op = static_cast<uint8_t>(ins.op);
            writePod(out, op);
            writePod(out, ins.a);
            writePod(out, ins.b);
            writePod(out, ins.line);
        }
    }

    if (!out.good()) {
        errorOut = "Greška pri zapisivanju bajtkod datoteke '" + path + "'.";
        return false;
    }
    return true;
}

bool BytecodeFormat::readFromFile(const std::string& path, BytecodeModule& moduleOut, std::string& errorOut) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        errorOut = "Nije moguće otvoriti bajtkod datoteku '" + path + "'.";
        return false;
    }

    uint32_t magic = 0;
    uint16_t version = 0;
    if (!readPod(in, magic) || magic != MAGIC) {
        errorOut = "Neispravan format BOS bajtkod datoteke (pogrešan zaglavni potpis).";
        return false;
    }
    if (!readPod(in, version) || version != FORMAT_VERSION) {
        errorOut = "Nepodržana verzija BOS bajtkod formata: " + std::to_string(version) + ".";
        return false;
    }

    BytecodeModule mod;
    if (!readString(in, mod.filename)) {
        errorOut = "Oštećena bajtkod datoteka (naziv).";
        return false;
    }

    uint32_t globalCount = 0;
    if (!readPod(in, globalCount)) return false;
    mod.globalNames.resize(globalCount);
    for (uint32_t i = 0; i < globalCount; ++i) {
        if (!readString(in, mod.globalNames[i])) return false;
    }

    uint32_t structCount = 0;
    if (!readPod(in, structCount)) return false;
    mod.structs.resize(structCount);
    for (uint32_t i = 0; i < structCount; ++i) {
        if (!readString(in, mod.structs[i].name)) return false;
        uint32_t fc = 0;
        if (!readPod(in, fc)) return false;
        mod.structs[i].fields.resize(fc);
        for (uint32_t j = 0; j < fc; ++j) {
            if (!readString(in, mod.structs[i].fields[j].name)) return false;
            if (!readPod(in, mod.structs[i].fields[j].typeTag)) return false;
        }
    }

    uint32_t classCount = 0;
    if (!readPod(in, classCount)) return false;
    mod.classes.resize(classCount);
    for (uint32_t i = 0; i < classCount; ++i) {
        if (!readString(in, mod.classes[i].name)) return false;
        if (!readString(in, mod.classes[i].baseClass)) return false;
        if (!readPod(in, mod.classes[i].constructorFuncIndex)) return false;
        uint32_t fc = 0;
        if (!readPod(in, fc)) return false;
        mod.classes[i].fields.resize(fc);
        for (uint32_t j = 0; j < fc; ++j) {
            if (!readString(in, mod.classes[i].fields[j].name)) return false;
            if (!readPod(in, mod.classes[i].fields[j].typeTag)) return false;
        }
        uint32_t mc = 0;
        if (!readPod(in, mc)) return false;
        for (uint32_t j = 0; j < mc; ++j) {
            std::string mname;
            int32_t fidx = -1;
            if (!readString(in, mname) || !readPod(in, fidx)) return false;
            mod.classes[i].methods[mname] = fidx;
        }
    }

    uint32_t fnCount = 0;
    if (!readPod(in, fnCount)) return false;
    mod.functions.resize(fnCount);
    for (uint32_t i = 0; i < fnCount; ++i) {
        auto& fn = mod.functions[i];
        uint8_t isCtor = 0;
        if (!readString(in, fn.name) || !readString(in, fn.ownerClass) ||
            !readPod(in, fn.paramCount) || !readPod(in, fn.localCount) ||
            !readPod(in, isCtor)) {
            errorOut = "Oštećena bajtkod datoteka (zaglavlje funkcije).";
            return false;
        }
        fn.isConstructor = (isCtor != 0);

        uint32_t cc = 0;
        if (!readPod(in, cc)) return false;
        fn.constants.resize(cc);
        for (uint32_t j = 0; j < cc; ++j) {
            uint8_t kind = 0;
            if (!readPod(in, kind)) return false;
            fn.constants[j].kind = static_cast<ConstKind>(kind);
            switch (fn.constants[j].kind) {
                case ConstKind::Prazno: break;
                case ConstKind::Cijeli:
                    if (!readPod(in, fn.constants[j].intVal)) return false;
                    break;
                case ConstKind::Decimalni:
                    if (!readPod(in, fn.constants[j].floatVal)) return false;
                    break;
                case ConstKind::Logicki: {
                    uint8_t bv = 0;
                    if (!readPod(in, bv)) return false;
                    fn.constants[j].boolVal = (bv != 0);
                    break;
                }
                case ConstKind::Znak: {
                    uint32_t cp = 0;
                    if (!readPod(in, cp)) return false;
                    fn.constants[j].charVal = static_cast<char32_t>(cp);
                    break;
                }
                case ConstKind::Tekst:
                    if (!readString(in, fn.constants[j].strVal)) return false;
                    break;
            }
        }

        uint32_t ic = 0;
        if (!readPod(in, ic)) return false;
        fn.code.resize(ic);
        for (uint32_t j = 0; j < ic; ++j) {
            uint8_t op = 0;
            if (!readPod(in, op) || !readPod(in, fn.code[j].a) ||
                !readPod(in, fn.code[j].b) || !readPod(in, fn.code[j].line)) {
                errorOut = "Oštećena bajtkod datoteka (instrukcija).";
                return false;
            }
            fn.code[j].op = static_cast<OpCode>(op);
        }
    }

    moduleOut = std::move(mod);
    return true;
}

} // namespace bos
