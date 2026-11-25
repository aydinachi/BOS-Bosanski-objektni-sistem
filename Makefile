CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Icompiler
BUILD_DIR := build
BIN_DIR := bin

CORE_SRCS := \
	compiler/diagnostics/diagnostic.cpp \
	compiler/lexer/token.cpp \
	compiler/lexer/lexer.cpp \
	compiler/types/type.cpp \
	compiler/ast/ast_printer.cpp \
	compiler/parser/parser.cpp \
	compiler/semantic/symbol_table.cpp \
	compiler/semantic/analyzer.cpp \
	compiler/ir/ir.cpp \
	compiler/ir/ir_generator.cpp \
	compiler/runtime/value.cpp \
	compiler/runtime/stdlib.cpp \
	compiler/backend/bytecode_format.cpp \
	compiler/backend/vm.cpp

CLI_SRC := compiler/cli/main.cpp
TEST_SRC := tests/test_runner.cpp

CORE_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CORE_SRCS))
CLI_OBJ := $(BUILD_DIR)/compiler/cli/main.o
TEST_OBJ := $(BUILD_DIR)/tests/test_runner.o

BOS_BIN := $(BIN_DIR)/bos
TEST_BIN := $(BIN_DIR)/bos_tests

.PHONY: all test clean

all: $(BOS_BIN)

$(BOS_BIN): $(CORE_OBJS) $(CLI_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TEST_BIN): $(CORE_OBJS) $(TEST_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TEST_BIN) $(BOS_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
