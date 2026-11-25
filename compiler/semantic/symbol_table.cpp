#include "semantic/symbol_table.hpp"

#include <unordered_set>

namespace bos {

const FieldInfo* StructInfo::findField(const std::string& fieldName) const {
    for (const auto& f : fields) {
        if (f.name == fieldName) return &f;
    }
    return nullptr;
}

SymbolTable::SymbolTable() {
    pushScope(); // global scope
}

void SymbolTable::pushScope() {
    scopes_.emplace_back();
}

void SymbolTable::popScope() {
    if (scopes_.size() > 1) {
        scopes_.pop_back();
    }
}

bool SymbolTable::declare(const Symbol& sym) {
    auto& current = scopes_.back();
    if (current.find(sym.name) != current.end()) {
        return false;
    }
    current.emplace(sym.name, sym);
    return true;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::lookupCurrentScope(const std::string& name) const {
    const auto& current = scopes_.back();
    auto found = current.find(name);
    if (found != current.end()) return &found->second;
    return nullptr;
}

bool SymbolTable::declareFunction(const FunctionSignature& sig) {
    if (functions_.find(sig.name) != functions_.end()) return false;
    functions_.emplace(sig.name, sig);
    return true;
}

const FunctionSignature* SymbolTable::lookupFunction(const std::string& name) const {
    auto it = functions_.find(name);
    if (it != functions_.end()) return &it->second;
    return nullptr;
}

bool SymbolTable::declareStruct(const StructInfo& info) {
    if (structs_.find(info.name) != structs_.end() || classes_.find(info.name) != classes_.end()) {
        return false;
    }
    structs_.emplace(info.name, info);
    return true;
}

const StructInfo* SymbolTable::lookupStruct(const std::string& name) const {
    auto it = structs_.find(name);
    if (it != structs_.end()) return &it->second;
    return nullptr;
}

StructInfo* SymbolTable::lookupStructMut(const std::string& name) {
    auto it = structs_.find(name);
    if (it != structs_.end()) return &it->second;
    return nullptr;
}

bool SymbolTable::declareClass(const ClassInfo& info) {
    if (classes_.find(info.name) != classes_.end() || structs_.find(info.name) != structs_.end()) {
        return false;
    }
    classes_.emplace(info.name, info);
    return true;
}

const ClassInfo* SymbolTable::lookupClass(const std::string& name) const {
    auto it = classes_.find(name);
    if (it != classes_.end()) return &it->second;
    return nullptr;
}

ClassInfo* SymbolTable::lookupClassMut(const std::string& name) {
    auto it = classes_.find(name);
    if (it != classes_.end()) return &it->second;
    return nullptr;
}

bool SymbolTable::isSubclassOf(const std::string& derived, const std::string& base) const {
    if (derived == base) return true;
    std::unordered_set<std::string> visited;
    std::string cur = derived;
    while (!cur.empty() && visited.insert(cur).second) {
        const ClassInfo* ci = lookupClass(cur);
        if (!ci) return false;
        if (ci->baseClass == base) return true;
        cur = ci->baseClass;
    }
    return false;
}

const FieldInfo* SymbolTable::resolveClassField(const std::string& className, const std::string& fieldName) const {
    std::unordered_set<std::string> visited;
    std::string cur = className;
    while (!cur.empty() && visited.insert(cur).second) {
        const ClassInfo* ci = lookupClass(cur);
        if (!ci) return nullptr;
        for (const auto& f : ci->fields) {
            if (f.name == fieldName) return &f;
        }
        cur = ci->baseClass;
    }
    return nullptr;
}

const FunctionSignature* SymbolTable::resolveClassMethod(const std::string& className, const std::string& methodName) const {
    std::unordered_set<std::string> visited;
    std::string cur = className;
    while (!cur.empty() && visited.insert(cur).second) {
        const ClassInfo* ci = lookupClass(cur);
        if (!ci) return nullptr;
        auto it = ci->methods.find(methodName);
        if (it != ci->methods.end()) return &it->second;
        cur = ci->baseClass;
    }
    return nullptr;
}

} // namespace bos
