# BOS Compiler Architecture & Developer Guide

## Directory Layout

```
compiler/
  diagnostics/   # SourceLocation, Diagnostic, DiagnosticEngine (Bosnian diagnostics)
  lexer/         # UTF-8 decoder, Token definitions, Bosnian keyword table, Lexer
  types/         # Static Type hierarchy (primitives, generics, zapis, klasa, functions)
  ast/           # Abstract Syntax Tree nodes & AST pretty-printer
  parser/        # Recursive-descent parser with operator precedence & recovery
  semantic/      # Lexical SymbolTable, Static Type Checker, Visibility/Inheritance Checker
  ir/            # BOS IR opcodes, BytecodeModule, Disassembler, AST->IR Code Generator
  runtime/       # Tagged runtime Value system & Standard Library (matematika, tekst, kolekcije, datoteke)
  backend/       # BOS Stack Bytecode Virtual Machine & .bosb Binary Serializer/Loader
  cli/           # Command-line interface (`bos pokreni|prevedi|provjeri|verzija|pomoc`)
tests/           # C++17 automated unit & integration test suite + .bos test programs
examples/        # Example .bos programs demonstrating all language features
docs/            # Language specification & architecture documentation
```

## Compilation Pipeline

```
.bos source (UTF-8)
  -> Lexer (compiler/lexer)
  -> Parser (compiler/parser) -> AST (compiler/ast)
  -> Semantic Analyzer & Type Checker (compiler/semantic, compiler/types)
  -> IR Bytecode Generator (compiler/ir) -> BytecodeModule
  -> [prevedi] Binary Bytecode Writer (compiler/backend/bytecode_format) -> .bosb file
  -> [pokreni] Bytecode Virtual Machine (compiler/backend/vm) + Runtime Stdlib (compiler/runtime)
```

## Building from Source

Requirements: `g++` (GCC 10+ or Clang 11+ with C++17 support) and `make`.

```bash
# Build the BOS compiler & CLI binary (bin/bos)
make

# Build and run the full automated test suite (bin/bos_tests)
make test

# Clean build artifacts
make clean
```

## CLI Usage

```bash
./bin/bos pokreni examples/dobrodosli.bos       # Compile to bytecode and execute on BOS VM
./bin/bos prevedi examples/dobrodosli.bos       # Compile to binary bytecode (.bosb)
./bin/bos pokreni examples/dobrodosli.bosb      # Execute a compiled .bosb bytecode binary
./bin/bos provjeri examples/dobrodosli.bos      # Run lexical, syntax, and semantic checks only
./bin/bos verzija                               # Print compiler version and backend info
./bin/bos pomoc                                 # Show CLI help in Bosnian
```

### Inspection Flags
- `--tokens`: Dump lexer token stream
- `--ast`: Dump Abstract Syntax Tree
- `--ir`: Dump disassembled BOS bytecode IR
- `--json`: Output structured JSON (diagnostics, stdout, tokens, AST, IR, timings) for IDE integration
- `--sandbox-dir <dir>`: Confine `datoteke` module I/O operations to a specific directory
