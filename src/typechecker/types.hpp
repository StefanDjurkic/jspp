#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace jspp {

// ============================================================================
// Resolved Type system (used by type checker, distinct from AST TypeNode)
// ============================================================================

enum class ResolvedTypeKind {
    Int, I8, I16, I32, I64,
    U8, U16, U32, U64,
    Float, Double,
    Bool, String, Char,
    Void,
    Array,
    Optional,
    Pointer,
    Struct,
    Class,
    Enum,
    Function,
    Generic,
    Auto,
    Error   // Sentinel for type errors
};

struct ResolvedType {
    ResolvedTypeKind kind;
    std::string name;

    // For Array, Optional, Pointer
    std::shared_ptr<ResolvedType> elementType;

    // For Struct/Class — field types
    std::unordered_map<std::string, std::shared_ptr<ResolvedType>> fields;

    // For Function
    std::vector<std::shared_ptr<ResolvedType>> paramTypes;
    std::shared_ptr<ResolvedType> returnType;

    // For Generic (template instantiation)
    std::vector<std::shared_ptr<ResolvedType>> typeArgs;

    bool isConst = false;
    bool isRef   = false;

    bool isNumeric() const {
        switch (kind) {
            case ResolvedTypeKind::Int:
            case ResolvedTypeKind::I8:  case ResolvedTypeKind::I16:
            case ResolvedTypeKind::I32: case ResolvedTypeKind::I64:
            case ResolvedTypeKind::U8:  case ResolvedTypeKind::U16:
            case ResolvedTypeKind::U32: case ResolvedTypeKind::U64:
            case ResolvedTypeKind::Float: case ResolvedTypeKind::Double:
                return true;
            default: return false;
        }
    }

    bool isInteger() const {
        switch (kind) {
            case ResolvedTypeKind::Int:
            case ResolvedTypeKind::I8:  case ResolvedTypeKind::I16:
            case ResolvedTypeKind::I32: case ResolvedTypeKind::I64:
            case ResolvedTypeKind::U8:  case ResolvedTypeKind::U16:
            case ResolvedTypeKind::U32: case ResolvedTypeKind::U64:
                return true;
            default: return false;
        }
    }

    bool isFloatingPoint() const {
        return kind == ResolvedTypeKind::Float || kind == ResolvedTypeKind::Double;
    }

    static std::shared_ptr<ResolvedType> makeInt() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Int;
        t->name = "int";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeDouble() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Double;
        t->name = "double";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeBool() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Bool;
        t->name = "bool";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeString() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::String;
        t->name = "string";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeChar() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Char;
        t->name = "char";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeVoid() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Void;
        t->name = "void";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeAuto() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Auto;
        t->name = "auto";
        return t;
    }

    static std::shared_ptr<ResolvedType> makeError() {
        auto t = std::make_shared<ResolvedType>();
        t->kind = ResolvedTypeKind::Error;
        t->name = "<error>";
        return t;
    }
};

} // namespace jspp
