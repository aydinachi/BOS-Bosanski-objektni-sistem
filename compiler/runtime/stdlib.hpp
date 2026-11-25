#pragma once

#include "runtime/value.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace bos {

struct BosRuntimeException : public std::runtime_error {
    int64_t code;
    BosRuntimeException(const std::string& msg, int64_t errCode = 1)
        : std::runtime_error(msg), code(errCode) {}
};

class StandardLibrary {
public:
    explicit StandardLibrary(std::string sandboxDir = "");

    Value callModuleFunction(const std::string& moduleName,
                             const std::string& funcName,
                             const std::vector<Value>& args);

    Value getModuleConstant(const std::string& moduleName,
                            const std::string& constName);

    Value callBuiltinMethod(Value& receiver,
                            const std::string& methodName,
                            const std::vector<Value>& args);

    // UTF-8 Bosnian text utilities
    static int64_t utf8Length(const std::string& s);
    static char32_t utf8CharAt(const std::string& s, int64_t index);
    static std::string utf8ToUpper(const std::string& s);
    static std::string utf8ToLower(const std::string& s);

private:
    std::string resolveSafePath(const std::string& userPath) const;

    std::string sandboxDir_;
};

} // namespace bos
