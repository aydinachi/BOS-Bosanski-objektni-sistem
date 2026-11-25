#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostic.hpp"
#include "types/type.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bos {

struct Symbol {
    std::string name;
    TypePtr type;
    bool isConst = false;
    SourceLocation location;
};

struct FunctionSignature {
    std::string name;
    std::vector<TypePtr> paramTypes;
    TypePtr returnType;
    Visibility visibility = Visibility::Javno;
    std::string ownerClass; // empty for free functions
    SourceLocation location;
};

struct FieldInfo {
    std::string name;
    TypePtr type;
    Visibility visibility = Visibility::Javno;
    std::string ownerClass;
    SourceLocation location;
};

struct StructInfo {
    std::string name;
    std::vector<FieldInfo> fields;
    SourceLocation location;

    const FieldInfo* findField(const std::string& fieldName) const;
};

struct ClassInfo {
    std::string name;
    std::string baseClass;
    std::vector<FieldInfo> fields;
    std::unordered_map<std::string, FunctionSignature> methods;
    std::optional<FunctionSignature> constructor;
    SourceLocation location;
};

class SymbolTable {
public:
    SymbolTable();

    void pushScope();
    void popScope();
    size_t depth() const { return scopes_.size(); }

    bool declare(const Symbol& sym);
    const Symbol* lookup(const std::string& name) const;
    const Symbol* lookupCurrentScope(const std::string& name) const;

    bool declareFunction(const FunctionSignature& sig);
    const FunctionSignature* lookupFunction(const std::string& name) const;

    bool declareStruct(const StructInfo& info);
    const StructInfo* lookupStruct(const std::string& name) const;
    StructInfo* lookupStructMut(const std::string& name);

    bool declareClass(const ClassInfo& info);
    const ClassInfo* lookupClass(const std::string& name) const;
    ClassInfo* lookupClassMut(const std::string& name);

    bool isSubclassOf(const std::string& derived, const std::string& base) const;
    const FieldInfo* resolveClassField(const std::string& className, const std::string& fieldName) const;
    const FunctionSignature* resolveClassMethod(const std::string& className, const std::string& methodName) const;

    const std::unordered_map<std::string, StructInfo>& structs() const { return structs_; }
    const std::unordered_map<std::string, ClassInfo>& classes() const { return classes_; }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, ClassInfo> classes_;
};

} // namespace bos
