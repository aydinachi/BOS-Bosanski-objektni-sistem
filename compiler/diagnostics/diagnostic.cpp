#include "diagnostics/diagnostic.hpp"

#include <sstream>

namespace bos {

void DiagnosticEngine::report(DiagnosticLevel level, DiagnosticStage stage,
                              const SourceLocation& loc, const std::string& message,
                              const std::string& code) {
    diagnostics_.push_back(Diagnostic{level, stage, loc, message, code});
}

void DiagnosticEngine::error(DiagnosticStage stage, const SourceLocation& loc,
                             const std::string& message, const std::string& code) {
    report(DiagnosticLevel::Error, stage, loc, message, code);
}

void DiagnosticEngine::warning(DiagnosticStage stage, const SourceLocation& loc,
                               const std::string& message, const std::string& code) {
    report(DiagnosticLevel::Warning, stage, loc, message, code);
}

bool DiagnosticEngine::hasErrors() const {
    for (const auto& d : diagnostics_) {
        if (d.level == DiagnosticLevel::Error) return true;
    }
    return false;
}

size_t DiagnosticEngine::errorCount() const {
    size_t count = 0;
    for (const auto& d : diagnostics_) {
        if (d.level == DiagnosticLevel::Error) ++count;
    }
    return count;
}

std::string DiagnosticEngine::stageName(DiagnosticStage stage) {
    switch (stage) {
        case DiagnosticStage::Lexer: return "Leksička greška";
        case DiagnosticStage::Parser: return "Sintaksna greška";
        case DiagnosticStage::Semantic: return "Semantička greška";
        case DiagnosticStage::CodeGen: return "Greška generisanja koda";
        case DiagnosticStage::Runtime: return "Greška izvršavanja";
    }
    return "Greška";
}

std::string DiagnosticEngine::levelName(DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::Error: return "greška";
        case DiagnosticLevel::Warning: return "upozorenje";
        case DiagnosticLevel::Note: return "napomena";
    }
    return "greška";
}

std::string DiagnosticEngine::formatDiagnostic(const Diagnostic& diag) {
    std::ostringstream oss;
    oss << diag.location.filename << ":" << diag.location.line << ":" << diag.location.column
        << ": " << stageName(diag.stage);
    if (!diag.code.empty()) {
        oss << " [" << diag.code << "]";
    }
    oss << ": " << diag.message;
    return oss.str();
}

std::string DiagnosticEngine::formatAll() const {
    std::ostringstream oss;
    for (size_t i = 0; i < diagnostics_.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << formatDiagnostic(diagnostics_[i]);
    }
    return oss.str();
}

} // namespace bos
