#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace bos {

enum class ConstKind : uint8_t {
    Prazno = 0,
    Cijeli = 1,
    Decimalni = 2,
    Logicki = 3,
    Znak = 4,
    Tekst = 5
};

struct Constant {
    ConstKind kind = ConstKind::Prazno;
    int64_t intVal = 0;
    double floatVal = 0.0;
    bool boolVal = false;
    char32_t charVal = 0;
    std::string strVal;

    static Constant prazno() { return Constant{}; }
    static Constant cijeli(int64_t v) { Constant c; c.kind = ConstKind::Cijeli; c.intVal = v; return c; }
    static Constant decimalni(double v) { Constant c; c.kind = ConstKind::Decimalni; c.floatVal = v; return c; }
    static Constant logicki(bool v) { Constant c; c.kind = ConstKind::Logicki; c.boolVal = v; return c; }
    static Constant znak(char32_t v) { Constant c; c.kind = ConstKind::Znak; c.charVal = v; return c; }
    static Constant tekst(std::string s) { Constant c; c.kind = ConstKind::Tekst; c.strVal = std::move(s); return c; }

    std::string toString() const;
};

enum class OpCode : uint8_t {
    PUSH_CONST = 1,      // a = const index
    PUSH_PRAZNO = 2,
    POP = 3,
    DUP = 4,

    LOAD_LOCAL = 10,     // a = local slot
    STORE_LOCAL = 11,    // a = local slot
    LOAD_GLOBAL = 12,    // a = global slot
    STORE_GLOBAL = 13,   // a = global slot
    LOAD_SELF = 14,

    ADD = 20,
    SUB = 21,
    MUL = 22,
    DIV = 23,
    MOD = 24,
    NEG = 25,
    NOT = 26,

    EQ = 30,
    NEQ = 31,
    LT = 32,
    LTE = 33,
    GT = 34,
    GTE = 35,

    JUMP = 40,           // a = target ip
    JUMP_IF_FALSE = 41,  // a = target ip (pops top)
    JUMP_IF_TRUE = 42,   // a = target ip (pops top)

    CALL_FUNC = 50,      // a = func index, b = argc
    CALL_METHOD = 51,    // a = method name const idx, b = argc (stack: receiver, arg0..argN-1)
    CALL_MODULE = 52,    // a = "mod.fn" const idx, b = argc
    CALL_BUILTIN = 53,   // a = method name const idx, b = argc (stack: receiver, arg0..argN-1)
    RETURN = 54,

    NEW_STRUCT = 60,     // a = struct index, b = argc (0 or fieldCount)
    NEW_CLASS = 61,      // a = class index, b = argc (calls constructor if defined)
    GET_FIELD = 62,      // a = field name const idx
    SET_FIELD = 63,      // a = field name const idx (stack: obj, val -> leaves nothing)

    BUILD_NIZ = 70,      // a = count
    BUILD_LISTA = 71,    // a = count
    BUILD_RJECNIK = 72,  // a = pair count
    GET_INDEX = 73,      // stack: obj, idx -> val
    SET_INDEX = 74,      // stack: obj, idx, val -> nothing
    ITER_LEN = 75,       // stack: iterable -> int len
    ITER_GET = 76,       // stack: iterable, int idx -> elem

    PRINT = 80,          // stack: val -> nothing
    INPUT = 81,          // a = typeTag (0=tekst, 1=cijeli, 2=decimalni, 3=logicki, 4=znak) -> pushes val
    IMPORT_MODULE = 82,  // a = module name const idx, b = global slot
    GET_MODULE_CONST = 83, // a = "mod.const" const idx

    PUSH_TRY = 90,       // a = catch ip
    POP_TRY = 91,
    THROW = 92,          // stack: msg or Greska

    HALT = 99
};

struct Instruction {
    OpCode op = OpCode::HALT;
    int32_t a = 0;
    int32_t b = 0;
    int32_t line = 0;
};

struct FunctionIR {
    std::string name;
    std::string ownerClass;
    int32_t paramCount = 0;
    int32_t localCount = 0;
    bool isConstructor = false;
    std::vector<Instruction> code;
    std::vector<Constant> constants;

    int32_t addConstant(const Constant& c);
    int32_t emit(OpCode op, int32_t a = 0, int32_t b = 0, int32_t line = 0);
};

struct FieldDefaultIR {
    std::string name;
    uint8_t typeTag = 0; // 0=prazno, 1=cijeli, 2=decimalni, 3=tekst, 4=znak, 5=logicki, 6=niz, 7=lista, 8=rjecnik
};

struct StructIR {
    std::string name;
    std::vector<FieldDefaultIR> fields;
};

struct ClassIR {
    std::string name;
    std::string baseClass;
    std::vector<FieldDefaultIR> fields;
    std::unordered_map<std::string, int32_t> methods;
    int32_t constructorFuncIndex = -1;
};

struct BytecodeModule {
    std::string filename;
    std::vector<FunctionIR> functions; // functions[0] is always __main__
    std::vector<StructIR> structs;
    std::vector<ClassIR> classes;
    std::vector<std::string> globalNames;

    int32_t findFunction(const std::string& name) const;
    int32_t findStruct(const std::string& name) const;
    int32_t findClass(const std::string& name) const;
    int32_t resolveClassMethod(const std::string& className, const std::string& methodName) const;

    std::string disassemble() const;
};

const char* opCodeName(OpCode op);

} // namespace bos
