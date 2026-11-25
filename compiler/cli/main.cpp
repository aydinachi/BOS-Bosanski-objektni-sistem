#include "ast/ast_printer.hpp"
#include "backend/bytecode_format.hpp"
#include "backend/vm.hpp"
#include "diagnostics/diagnostic.hpp"
#include "ir/ir_generator.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "semantic/analyzer.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

bool readFileUtf8(const std::string& path, std::string& outContent) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream oss;
    oss << in.rdbuf();
    outContent = oss.str();
    return true;
}

void printVersion() {
    std::cout << "BOS 0.1.0 (Bosanski objektni sistem)\n"
              << "Kompajler: C++17 (UTF-8 Lexer, Parser, AST, Statički analizator, IR bajtkod generator)\n"
              << "Izvršno okruženje: BOS Bajtkod Virtuelna Mašina (VM) + .bosb binarni bajtkod\n"
              << "Napomena: Nativni LLVM backend nije uključen u ovo izdanje (LLVM nije dostupan u okruženju).\n";
}

void printHelp() {
    std::cout << "BOS — Bosanski objektni sistem (v0.1.0)\n\n"
              << "Korištenje:\n"
              << "  bos pokreni <datoteka.bos | datoteka.bosb>   Pokreće BOS program na bajtkod VM-u\n"
              << "  bos prevedi <datoteka.bos> [-o izlaz.bosb]   Prevodi izvor u BOS bajtkod (.bosb)\n"
              << "  bos provjeri <datoteka.bos>                  Vrši leksičku, sintaksnu i semantičku provjeru\n"
              << "  bos verzija                                  Prikazuje verziju BOS kompajlera\n"
              << "  bos pomoc                                    Prikazuje ovu pomoć\n\n"
              << "Dodatne opcije za inspekciju:\n"
              << "  --tokens              Prikazuje tokene leksera\n"
              << "  --ast                 Prikazuje apstraktno sintaksno stablo (AST)\n"
              << "  --ir                  Prikazuje generisani BOS bajtkod IR\n"
              << "  --json                Izlaz u strukturiranom JSON formatu (za IDE)\n"
              << "  --sandbox-dir <dir>   Ograničava modul 'datoteke' na zadani direktorij\n";
}

std::string defaultOutputPath(const std::string& inputPath) {
    if (inputPath.size() > 4 && inputPath.substr(inputPath.size() - 4) == ".bos") {
        return inputPath.substr(0, inputPath.size() - 4) + ".bosb";
    }
    return inputPath + ".bosb";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp();
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "verzija" || cmd == "--version" || cmd == "-v") {
        printVersion();
        return 0;
    }
    if (cmd == "pomoc" || cmd == "pomoć" || cmd == "--help" || cmd == "-h") {
        printHelp();
        return 0;
    }

    if (cmd != "pokreni" && cmd != "prevedi" && cmd != "provjeri") {
        std::cerr << "Nepoznata naredba '" << cmd << "'. Pokrenite 'bos pomoc' za listu naredbi.\n";
        return 1;
    }

    std::string inputPath;
    std::string outputPath;
    std::string sandboxDir;
    bool flagTokens = false;
    bool flagAst = false;
    bool flagIr = false;
    bool flagJson = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tokens") flagTokens = true;
        else if (arg == "--ast") flagAst = true;
        else if (arg == "--ir") flagIr = true;
        else if (arg == "--json") flagJson = true;
        else if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
        else if (arg == "--sandbox-dir" && i + 1 < argc) sandboxDir = argv[++i];
        else if (inputPath.empty()) inputPath = arg;
    }

    if (inputPath.empty()) {
        std::cerr << "Greška: nije navedena ulazna .bos datoteka.\n";
        return 1;
    }

    auto startTime = std::chrono::steady_clock::now();
    bos::DiagnosticEngine diagnostics;
    std::vector<bos::Token> tokens;
    std::string astText;
    std::string irText;
    std::string programOutput;
    std::string compileNote;
    std::string compiledArtifactPath;
    uint64_t stepsExecuted = 0;
    bool success = false;

    // Direct execution of precompiled .bosb bytecode binary
    if (cmd == "pokreni" && bos::BytecodeFormat::isBytecodeFile(inputPath)) {
        bos::BytecodeModule module;
        std::string err;
        if (!bos::BytecodeFormat::readFromFile(inputPath, module, err)) {
            diagnostics.error(bos::DiagnosticStage::Runtime, {inputPath, 1, 1}, err, "B001");
        } else {
            if (flagIr || flagJson) irText = module.disassemble();
            std::ostringstream outBuf;
            bos::VMOptions vmOpts;
            vmOpts.input = &std::cin;
            vmOpts.output = flagJson ? static_cast<std::ostream*>(&outBuf) : static_cast<std::ostream*>(&std::cout);
            vmOpts.sandboxDir = sandboxDir;
            bos::VirtualMachine vm(vmOpts);
            auto vmRes = vm.execute(module, diagnostics);
            stepsExecuted = vmRes.stepsExecuted;
            success = vmRes.ok;
            if (flagJson) programOutput = outBuf.str();
        }
    } else {
        std::string source;
        if (!readFileUtf8(inputPath, source)) {
            diagnostics.error(bos::DiagnosticStage::Lexer, {inputPath, 1, 1},
                              "Nije moguće otvoriti izvornu datoteku '" + inputPath + "'.", "IO01");
        } else {
            // Stage 1: Lexer
            bos::Lexer lexer(source, inputPath, diagnostics);
            tokens = lexer.tokenize();

            // Stage 2: Parser
            bos::Parser parser(tokens, inputPath, diagnostics);
            bos::Program program = parser.parseProgram();
            if (flagAst || flagJson) {
                astText = bos::AstPrinter::print(program);
            }

            // Stage 3: Semantic Analysis & Static Type Checking
            bos::SemanticAnalyzer analyzer(diagnostics);
            if (!diagnostics.hasErrors()) {
                analyzer.analyze(program);
            }

            if (!diagnostics.hasErrors()) {
                if (cmd == "provjeri") {
                    success = true;
                    if (flagIr || flagJson) {
                        bos::IRGenerator irGen(analyzer.symbols(), diagnostics);
                        auto mod = irGen.generate(program);
                        irText = mod.disassemble();
                    }
                } else {
                    // Stage 4: IR / Bytecode Generation
                    bos::IRGenerator irGen(analyzer.symbols(), diagnostics);
                    bos::BytecodeModule module = irGen.generate(program);
                    if (flagIr || flagJson) {
                        irText = module.disassemble();
                    }

                    if (cmd == "prevedi") {
                        std::string target = outputPath.empty() ? defaultOutputPath(inputPath) : outputPath;
                        std::string writeErr;
                        if (bos::BytecodeFormat::writeToFile(module, target, writeErr)) {
                            success = true;
                            compiledArtifactPath = target;
                            compileNote = "Uspješno prevedeno u BOS binarni bajtkod (.bosb): " + target +
                                          "\nNapomena: Pokrenite pomoću 'bos pokreni " + target +
                                          "'. Nativni LLVM izvršni backend još nije dostupan u ovom okruženju.";
                        } else {
                            diagnostics.error(bos::DiagnosticStage::CodeGen, {inputPath, 1, 1}, writeErr, "C001");
                        }
                    } else if (cmd == "pokreni") {
                        std::ostringstream outBuf;
                        bos::VMOptions vmOpts;
                        vmOpts.input = &std::cin;
                        vmOpts.output = flagJson ? static_cast<std::ostream*>(&outBuf) : static_cast<std::ostream*>(&std::cout);
                        vmOpts.sandboxDir = sandboxDir;
                        bos::VirtualMachine vm(vmOpts);
                        auto vmRes = vm.execute(module, diagnostics);
                        stepsExecuted = vmRes.stepsExecuted;
                        success = vmRes.ok;
                        if (flagJson) programOutput = outBuf.str();
                    }
                }
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    if (flagJson) {
        std::cout << "{\n";
        std::cout << "  \"ok\": " << (success ? "true" : "false") << ",\n";
        std::cout << "  \"command\": \"" << jsonEscape(cmd) << "\",\n";
        std::cout << "  \"backend\": \"BOS Bytecode VM v0.1 (.bosb)\",\n";
        std::cout << "  \"elapsedMs\": " << elapsedMs << ",\n";
        std::cout << "  \"stepsExecuted\": " << stepsExecuted << ",\n";
        std::cout << "  \"output\": \"" << jsonEscape(programOutput) << "\",\n";
        std::cout << "  \"compileNote\": \"" << jsonEscape(compileNote) << "\",\n";
        std::cout << "  \"artifactPath\": \"" << jsonEscape(compiledArtifactPath) << "\",\n";
        std::cout << "  \"ast\": \"" << jsonEscape(astText) << "\",\n";
        std::cout << "  \"ir\": \"" << jsonEscape(irText) << "\",\n";
        std::cout << "  \"tokens\": [\n";
        bool firstTok = true;
        for (const auto& t : tokens) {
            if (t.type == bos::TokenType::Newline || t.type == bos::TokenType::EndOfFile) continue;
            if (!firstTok) std::cout << ",\n";
            firstTok = false;
            std::cout << "    {\"type\": \"" << bos::tokenTypeName(t.type)
                      << "\", \"lexeme\": \"" << jsonEscape(t.lexeme)
                      << "\", \"line\": " << t.location.line
                      << ", \"column\": " << t.location.column << "}";
        }
        std::cout << "\n  ],\n";
        std::cout << "  \"diagnostics\": [\n";
        for (size_t i = 0; i < diagnostics.diagnostics().size(); ++i) {
            const auto& d = diagnostics.diagnostics()[i];
            if (i > 0) std::cout << ",\n";
            std::cout << "    {\"level\": \"" << bos::DiagnosticEngine::levelName(d.level)
                      << "\", \"stage\": \"" << jsonEscape(bos::DiagnosticEngine::stageName(d.stage))
                      << "\", \"code\": \"" << jsonEscape(d.code)
                      << "\", \"file\": \"" << jsonEscape(d.location.filename)
                      << "\", \"line\": " << d.location.line
                      << ", \"column\": " << d.location.column
                      << ", \"message\": \"" << jsonEscape(d.message)
                      << "\", \"formatted\": \"" << jsonEscape(bos::DiagnosticEngine::formatDiagnostic(d)) << "\"}";
        }
        std::cout << "\n  ]\n";
        std::cout << "}\n";
        return success ? 0 : 1;
    }

    if (flagTokens) {
        std::cout << "=== TOKENI ===\n";
        for (const auto& t : tokens) {
            if (t.type == bos::TokenType::Newline) continue;
            std::cout << t.location.line << ":" << t.location.column << "\t"
                      << bos::tokenTypeName(t.type) << "\t" << t.lexeme << "\n";
        }
    }
    if (flagAst && !astText.empty()) {
        std::cout << "=== AST ===\n" << astText;
    }
    if (flagIr && !irText.empty()) {
        std::cout << "=== BOS BAJTKOD IR ===\n" << irText;
    }

    if (diagnostics.hasErrors()) {
        std::cerr << diagnostics.formatAll() << "\n";
        return 1;
    }

    if (cmd == "provjeri") {
        std::cout << "Provjera uspješna: '" << inputPath << "' nema leksičkih, sintaksnih ni semantičkih grešaka.\n";
    } else if (cmd == "prevedi") {
        std::cout << compileNote << "\n";
    }

    return success ? 0 : 1;
}
