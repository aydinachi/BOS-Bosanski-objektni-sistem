#pragma once

#include <string>
#include <vector>

namespace bos {

struct SourceLocation {
    std::string filename = "<ulaz>";
    int line = 1;
    int column = 1;
};

enum class DiagnosticLevel {
    Error,
    Warning,
    Note
};

enum class DiagnosticStage {
    Lexer,
    Parser,
    Semantic,
    CodeGen,
    Runtime
};

struct Diagnostic {
    DiagnosticLevel level = DiagnosticLevel::Error;
    DiagnosticStage stage = DiagnosticStage::Lexer;
    SourceLocation location;
    std::string message;
    std::string code;
};

class DiagnosticEngine {
public:
    void report(DiagnosticLevel level, DiagnosticStage stage,
                const SourceLocation& loc, const std::string& message,
                const std::string& code = "");

    void error(DiagnosticStage stage, const SourceLocation& loc,
               const std::string& message, const std::string& code = "");

    void warning(DiagnosticStage stage, const SourceLocation& loc,
                 const std::string& message, const std::string& code = "");

    bool hasErrors() const;
    size_t errorCount() const;
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    void clear() { diagnostics_.clear(); }

    std::string formatAll() const;
    static std::string formatDiagnostic(const Diagnostic& diag);
    static std::string stageName(DiagnosticStage stage);
    static std::string levelName(DiagnosticLevel level);

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace bos
