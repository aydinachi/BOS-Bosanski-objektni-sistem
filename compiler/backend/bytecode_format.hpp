#pragma once

#include "ir/ir.hpp"

#include <string>

namespace bos {

class BytecodeFormat {
public:
    static constexpr uint32_t MAGIC = 0x42534F42; // "BOSB" little-endian
    static constexpr uint16_t FORMAT_VERSION = 1;

    static bool writeToFile(const BytecodeModule& module, const std::string& path, std::string& errorOut);
    static bool readFromFile(const std::string& path, BytecodeModule& moduleOut, std::string& errorOut);
    static bool isBytecodeFile(const std::string& path);
};

} // namespace bos
