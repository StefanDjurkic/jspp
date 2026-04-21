#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "types.hpp"

namespace jspp {

enum class SymbolKind {
    Variable,
    Function,
    Type,
    Class,
    Enum,
    EnumMember,
    Parameter
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    std::shared_ptr<ResolvedType> type;
    bool isMutable = true;
    bool isInitialized = false;
};

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr)
        : parent_(std::move(parent)) {}

    bool declare(const std::string& name, Symbol symbol) {
        if (symbols_.count(name)) return false;
        symbols_[name] = std::move(symbol);
        return true;
    }

    Symbol* lookup(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) return &it->second;
        if (parent_) return parent_->lookup(name);
        return nullptr;
    }

    Symbol* lookupLocal(const std::string& name) {
        auto it = symbols_.find(name);
        if (it != symbols_.end()) return &it->second;
        return nullptr;
    }

    std::shared_ptr<Scope> parent() const { return parent_; }

    std::shared_ptr<Scope> createChild() {
        return std::make_shared<Scope>(std::shared_ptr<Scope>(this, [](Scope*){}));
    }

private:
    std::shared_ptr<Scope> parent_;
    std::unordered_map<std::string, Symbol> symbols_;
};

} // namespace jspp
