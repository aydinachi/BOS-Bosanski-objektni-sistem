#pragma once

#include "diagnostics/diagnostic.hpp"
#include "ir/ir.hpp"
#include "runtime/stdlib.hpp"
#include "runtime/value.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace bos {

struct VMOptions {
    std::istream* input = &std::cin;
    std::ostream* output = &std::cout;
    std::string sandboxDir;
    uint64_t maxSteps = 5000000;
    size_t maxCallDepth = 512;
};

struct VMResult {
    bool ok = true;
    uint64_t stepsExecuted = 0;
    std::string runtimeError;
    int32_t errorLine = 0;
};

class VirtualMachine {
public:
    explicit VirtualMachine(const VMOptions& options = VMOptions{});

    VMResult execute(const BytecodeModule& module, DiagnosticEngine& diagnostics);

private:
    struct CallFrame {
        int32_t funcIndex = 0;
        size_t ip = 0;
        std::vector<Value> locals;
        Value self;
        bool returnSelfOnExit = false;
    };

    struct TryHandler {
        size_t frameDepth = 0;
        size_t catchIp = 0;
        size_t stackDepth = 0;
    };

    Value constantToValue(const Constant& c) const;
    Value defaultValueForTag(uint8_t tag) const;
    Value pop();
    bool dispatchException(const Value& errVal,
                           std::vector<CallFrame>& frames,
                           std::vector<TryHandler>& handlers);

    VMOptions options_;
    StandardLibrary stdlib_;
    std::vector<Value> stack_;
    std::vector<Value> globals_;
};

} // namespace bos
