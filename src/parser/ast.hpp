#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "../lexer/token.hpp"

namespace jspp {

// Forward declarations
struct ASTNode;
struct Expression;
struct Statement;
struct TypeNode;
struct BlockStmt;

// ============================================================================
// Base Types
// ============================================================================

using ASTNodePtr  = std::unique_ptr<ASTNode>;
using ExprPtr     = std::unique_ptr<Expression>;
using StmtPtr     = std::unique_ptr<Statement>;
using TypeNodePtr = std::unique_ptr<TypeNode>;

// ============================================================================
// Type Representation in AST
// ============================================================================

enum class TypeKind {
    Primitive,    // int, double, bool, string, char, void, etc.
    Array,        // T[]
    Optional,     // T?
    Pointer,      // ptr<T>
    Named,        // User-defined type (struct/class name)
    Generic,      // Container<T>
    Function,     // (params) => return
    Auto,         // Compiler-inferred
    Unresolved    // Not yet resolved by type checker
};

struct TypeNode {
    TypeKind kind;
    std::string name;                         // "int", "string", "Point", etc.
    std::vector<TypeNodePtr> typeArgs;        // Generic type arguments <T, U>
    TypeNodePtr elementType;                  // For Array, Optional, Pointer
    std::vector<TypeNodePtr> paramTypes;      // For Function type
    TypeNodePtr returnType;                   // For Function type
    bool isNullable = false;
    SourceLocation loc;

    static TypeNodePtr makePrimitive(const std::string& name, SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Primitive;
        t->name = name;
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makeNamed(const std::string& name, SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Named;
        t->name = name;
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makeArray(TypeNodePtr elementType, SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Array;
        t->name = "Array";
        t->elementType = std::move(elementType);
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makeOptional(TypeNodePtr inner, SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Optional;
        t->name = "Optional";
        t->elementType = std::move(inner);
        t->isNullable = true;
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makePointer(TypeNodePtr inner, SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Pointer;
        t->name = "ptr";
        t->elementType = std::move(inner);
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makeAuto(SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Auto;
        t->name = "auto";
        t->loc = loc;
        return t;
    }

    static TypeNodePtr makeUnresolved(SourceLocation loc = {}) {
        auto t = std::make_unique<TypeNode>();
        t->kind = TypeKind::Unresolved;
        t->name = "<unresolved>";
        t->loc = loc;
        return t;
    }
};

// ============================================================================
// AST Node Base
// ============================================================================

enum class NodeKind {
    // Program
    Program,

    // Statements
    VariableDecl,
    FunctionDecl,
    TypeDecl,
    ClassDecl,
    EnumDecl,
    ImportDecl,
    ExportDecl,
    ReturnStmt,
    IfStmt,
    ForStmt,
    ForOfStmt,
    WhileStmt,
    DoWhileStmt,
    SwitchStmt,
    TryStmt,
    ThrowStmt,
    BreakStmt,
    ContinueStmt,
    BlockStmt,
    ExpressionStmt,
    CppBlock,
    CppInclude,

    // Expressions
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,
    BoolLiteral,
    NullLiteral,
    TemplateLiteral,
    Identifier,
    BinaryExpr,
    UnaryExpr,
    PostfixExpr,
    AssignExpr,
    CallExpr,
    MemberExpr,
    IndexExpr,
    ArrayLiteral,
    ObjectLiteral,
    ArrowFunction,
    FunctionExpr,
    NewExpr,
    SharedNewExpr,
    TernaryExpr,
    ThisExpr,
    SuperExpr,
    DestructureObj,
    DestructureArr,

    // Type/Class members
    FieldDecl,
    MethodDecl,
    ConstructorDecl,
    OperatorDecl,
    CaseClause,
    DefaultClause,
    CatchClause,
    Param,
    EnumMember,
};

struct ASTNode {
    NodeKind kind;
    SourceLocation loc;
    TypeNodePtr resolvedType;  // Filled by type checker

    virtual ~ASTNode() = default;

protected:
    ASTNode(NodeKind kind, SourceLocation loc = {})
        : kind(kind), loc(loc) {}
};

// ============================================================================
// Expressions
// ============================================================================

struct Expression : ASTNode {
    using ASTNode::ASTNode;
};

struct IntLiteralExpr : Expression {
    int64_t value;
    IntLiteralExpr(int64_t val, SourceLocation loc = {})
        : Expression(NodeKind::IntLiteral, loc), value(val) {}
};

struct FloatLiteralExpr : Expression {
    double value;
    FloatLiteralExpr(double val, SourceLocation loc = {})
        : Expression(NodeKind::FloatLiteral, loc), value(val) {}
};

struct StringLiteralExpr : Expression {
    std::string value;
    StringLiteralExpr(std::string val, SourceLocation loc = {})
        : Expression(NodeKind::StringLiteral, loc), value(std::move(val)) {}
};

struct CharLiteralExpr : Expression {
    char value;
    CharLiteralExpr(char val, SourceLocation loc = {})
        : Expression(NodeKind::CharLiteral, loc), value(val) {}
};

struct BoolLiteralExpr : Expression {
    bool value;
    BoolLiteralExpr(bool val, SourceLocation loc = {})
        : Expression(NodeKind::BoolLiteral, loc), value(val) {}
};

struct NullLiteralExpr : Expression {
    NullLiteralExpr(SourceLocation loc = {})
        : Expression(NodeKind::NullLiteral, loc) {}
};

struct IdentifierExpr : Expression {
    std::string name;
    IdentifierExpr(std::string name, SourceLocation loc = {})
        : Expression(NodeKind::Identifier, loc), name(std::move(name)) {}
};

struct ThisExpr : Expression {
    ThisExpr(SourceLocation loc = {})
        : Expression(NodeKind::ThisExpr, loc) {}
};

struct SuperExpr : Expression {
    SuperExpr(SourceLocation loc = {})
        : Expression(NodeKind::SuperExpr, loc) {}
};

struct BinaryExpr : Expression {
    std::string op;  // "+", "-", "===", "!==", etc.
    ExprPtr left;
    ExprPtr right;
    BinaryExpr(std::string op, ExprPtr left, ExprPtr right, SourceLocation loc = {})
        : Expression(NodeKind::BinaryExpr, loc), op(std::move(op)),
          left(std::move(left)), right(std::move(right)) {}
};

struct UnaryExpr : Expression {
    std::string op;  // "!", "-", "~", "++", "--"
    ExprPtr operand;
    bool prefix;     // true = prefix, false = postfix
    UnaryExpr(std::string op, ExprPtr operand, bool prefix, SourceLocation loc = {})
        : Expression(NodeKind::UnaryExpr, loc), op(std::move(op)),
          operand(std::move(operand)), prefix(prefix) {}
};

struct AssignExpr : Expression {
    std::string op;  // "=", "+=", "-=", etc.
    ExprPtr target;
    ExprPtr value;
    AssignExpr(std::string op, ExprPtr target, ExprPtr value, SourceLocation loc = {})
        : Expression(NodeKind::AssignExpr, loc), op(std::move(op)),
          target(std::move(target)), value(std::move(value)) {}
};

struct CallExpr : Expression {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args, SourceLocation loc = {})
        : Expression(NodeKind::CallExpr, loc), callee(std::move(callee)),
          args(std::move(args)) {}
};

struct MemberExpr : Expression {
    ExprPtr object;
    std::string member;
    MemberExpr(ExprPtr object, std::string member, SourceLocation loc = {})
        : Expression(NodeKind::MemberExpr, loc), object(std::move(object)),
          member(std::move(member)) {}
};

struct IndexExpr : Expression {
    ExprPtr object;
    ExprPtr index;
    IndexExpr(ExprPtr object, ExprPtr index, SourceLocation loc = {})
        : Expression(NodeKind::IndexExpr, loc), object(std::move(object)),
          index(std::move(index)) {}
};

struct ArrayLiteralExpr : Expression {
    std::vector<ExprPtr> elements;
    ArrayLiteralExpr(std::vector<ExprPtr> elements, SourceLocation loc = {})
        : Expression(NodeKind::ArrayLiteral, loc), elements(std::move(elements)) {}
};

struct ObjectField {
    std::string name;
    ExprPtr value;
    SourceLocation loc;
};

struct ObjectLiteralExpr : Expression {
    std::vector<ObjectField> fields;
    ObjectLiteralExpr(std::vector<ObjectField> fields, SourceLocation loc = {})
        : Expression(NodeKind::ObjectLiteral, loc), fields(std::move(fields)) {}
};

struct TemplateLiteralExpr : Expression {
    // Parts alternate: string, expr, string, expr, string...
    std::vector<std::string> strings;
    std::vector<ExprPtr> expressions;
    TemplateLiteralExpr(std::vector<std::string> strings,
                        std::vector<ExprPtr> expressions,
                        SourceLocation loc = {})
        : Expression(NodeKind::TemplateLiteral, loc),
          strings(std::move(strings)), expressions(std::move(expressions)) {}
};

struct TernaryExpr : Expression {
    ExprPtr condition;
    ExprPtr thenExpr;
    ExprPtr elseExpr;
    TernaryExpr(ExprPtr cond, ExprPtr then_, ExprPtr else_, SourceLocation loc = {})
        : Expression(NodeKind::TernaryExpr, loc), condition(std::move(cond)),
          thenExpr(std::move(then_)), elseExpr(std::move(else_)) {}
};

struct NewExprNode : Expression {
    std::string typeName;
    std::vector<TypeNodePtr> typeArgs;
    std::vector<ExprPtr> args;
    NewExprNode(std::string typeName, std::vector<ExprPtr> args, SourceLocation loc = {})
        : Expression(NodeKind::NewExpr, loc), typeName(std::move(typeName)),
          args(std::move(args)) {}
};

struct SharedNewExprNode : Expression {
    std::string typeName;
    std::vector<TypeNodePtr> typeArgs;
    std::vector<ExprPtr> args;
    SharedNewExprNode(std::string typeName, std::vector<ExprPtr> args, SourceLocation loc = {})
        : Expression(NodeKind::SharedNewExpr, loc), typeName(std::move(typeName)),
          args(std::move(args)) {}
};

// ============================================================================
// Parameters
// ============================================================================

struct ParamNode : ASTNode {
    std::string name;
    TypeNodePtr typeAnnotation;
    ExprPtr defaultValue;
    bool isRef = false;

    ParamNode(std::string name, TypeNodePtr type, ExprPtr defaultVal = nullptr,
              bool isRef = false, SourceLocation loc = {})
        : ASTNode(NodeKind::Param, loc), name(std::move(name)),
          typeAnnotation(std::move(type)), defaultValue(std::move(defaultVal)),
          isRef(isRef) {}
};

// ============================================================================
// Arrow Functions (as expressions)
// ============================================================================

struct ArrowFunctionExpr : Expression {
    std::vector<std::unique_ptr<ParamNode>> params;
    TypeNodePtr returnType;
    ExprPtr bodyExpr;                          // Single expression body
    std::vector<StmtPtr> bodyBlock;           // Block body (if braces used)
    bool hasBlockBody = false;

    ArrowFunctionExpr(SourceLocation loc = {})
        : Expression(NodeKind::ArrowFunction, loc) {}
};

// Anonymous function expression: function(a, b) { ... }
// Also covers named function expressions: function name(a, b) { ... }
struct FunctionExprNode : Expression {
    std::string name;  // Optional — empty for anonymous
    std::vector<std::unique_ptr<ParamNode>> params;
    TypeNodePtr returnType;
    std::unique_ptr<BlockStmt> body;

    FunctionExprNode(SourceLocation loc = {})
        : Expression(NodeKind::FunctionExpr, loc) {}
};

// ============================================================================
// Statements
// ============================================================================

struct Statement : ASTNode {
    using ASTNode::ASTNode;
};

struct BlockStmt : Statement {
    std::vector<StmtPtr> statements;
    BlockStmt(std::vector<StmtPtr> stmts, SourceLocation loc = {})
        : Statement(NodeKind::BlockStmt, loc), statements(std::move(stmts)) {}
};

struct ExpressionStmt : Statement {
    ExprPtr expr;
    ExpressionStmt(ExprPtr expr, SourceLocation loc = {})
        : Statement(NodeKind::ExpressionStmt, loc), expr(std::move(expr)) {}
};

struct VariableDeclStmt : Statement {
    std::string name;
    bool isConst;
    TypeNodePtr typeAnnotation;  // nullptr if inferred
    ExprPtr initializer;         // nullptr if uninitialized (typed-only)

    // Destructuring variants
    std::vector<std::string> destructNames;
    bool isObjDestructure = false;
    bool isArrDestructure = false;

    VariableDeclStmt(std::string name, bool isConst, TypeNodePtr type,
                     ExprPtr init, SourceLocation loc = {})
        : Statement(NodeKind::VariableDecl, loc), name(std::move(name)),
          isConst(isConst), typeAnnotation(std::move(type)),
          initializer(std::move(init)) {}
};

struct FunctionDeclStmt : Statement {
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<std::unique_ptr<ParamNode>> params;
    TypeNodePtr returnType;
    std::unique_ptr<BlockStmt> body;
    bool isExported = false;

    FunctionDeclStmt(std::string name, SourceLocation loc = {})
        : Statement(NodeKind::FunctionDecl, loc), name(std::move(name)) {}
};

struct ReturnStmt_ : Statement {
    ExprPtr value;
    ReturnStmt_(ExprPtr value = nullptr, SourceLocation loc = {})
        : Statement(NodeKind::ReturnStmt, loc), value(std::move(value)) {}
};

struct IfStmt_ : Statement {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::vector<std::pair<ExprPtr, std::unique_ptr<BlockStmt>>> elseIfBlocks;
    std::unique_ptr<BlockStmt> elseBlock;

    IfStmt_(SourceLocation loc = {})
        : Statement(NodeKind::IfStmt, loc) {}
};

struct ForStmt_ : Statement {
    StmtPtr init;      // Variable decl or expression
    ExprPtr condition;
    ExprPtr update;
    std::unique_ptr<BlockStmt> body;

    ForStmt_(SourceLocation loc = {})
        : Statement(NodeKind::ForStmt, loc) {}
};

struct ForOfStmt_ : Statement {
    std::string varName;
    bool isConst;
    ExprPtr iterable;
    std::unique_ptr<BlockStmt> body;

    ForOfStmt_(SourceLocation loc = {})
        : Statement(NodeKind::ForOfStmt, loc) {}
};

struct WhileStmt_ : Statement {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;

    WhileStmt_(SourceLocation loc = {})
        : Statement(NodeKind::WhileStmt, loc) {}
};

struct DoWhileStmt_ : Statement {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;

    DoWhileStmt_(SourceLocation loc = {})
        : Statement(NodeKind::DoWhileStmt, loc) {}
};

struct CaseClauseNode : ASTNode {
    ExprPtr value;
    std::vector<StmtPtr> body;
    CaseClauseNode(ExprPtr value, std::vector<StmtPtr> body, SourceLocation loc = {})
        : ASTNode(NodeKind::CaseClause, loc), value(std::move(value)),
          body(std::move(body)) {}
};

struct DefaultClauseNode : ASTNode {
    std::vector<StmtPtr> body;
    DefaultClauseNode(std::vector<StmtPtr> body, SourceLocation loc = {})
        : ASTNode(NodeKind::DefaultClause, loc), body(std::move(body)) {}
};

struct SwitchStmt_ : Statement {
    ExprPtr discriminant;
    std::vector<std::unique_ptr<CaseClauseNode>> cases;
    std::unique_ptr<DefaultClauseNode> defaultCase;

    SwitchStmt_(SourceLocation loc = {})
        : Statement(NodeKind::SwitchStmt, loc) {}
};

struct CatchClauseNode : ASTNode {
    std::string paramName;
    TypeNodePtr paramType;
    std::unique_ptr<BlockStmt> body;
    CatchClauseNode(SourceLocation loc = {})
        : ASTNode(NodeKind::CatchClause, loc) {}
};

struct TryStmt_ : Statement {
    std::unique_ptr<BlockStmt> tryBlock;
    std::unique_ptr<CatchClauseNode> catchClause;

    TryStmt_(SourceLocation loc = {})
        : Statement(NodeKind::TryStmt, loc) {}
};

struct ThrowStmt_ : Statement {
    ExprPtr value;
    ThrowStmt_(ExprPtr value, SourceLocation loc = {})
        : Statement(NodeKind::ThrowStmt, loc), value(std::move(value)) {}
};

struct BreakStmt_ : Statement {
    BreakStmt_(SourceLocation loc = {})
        : Statement(NodeKind::BreakStmt, loc) {}
};

struct ContinueStmt_ : Statement {
    ContinueStmt_(SourceLocation loc = {})
        : Statement(NodeKind::ContinueStmt, loc) {}
};

// ============================================================================
// Type / Struct Declarations
// ============================================================================

struct FieldDeclNode : ASTNode {
    std::string name;
    TypeNodePtr type;
    ExprPtr defaultValue;

    FieldDeclNode(std::string name, TypeNodePtr type, ExprPtr defaultVal = nullptr,
                  SourceLocation loc = {})
        : ASTNode(NodeKind::FieldDecl, loc), name(std::move(name)),
          type(std::move(type)), defaultValue(std::move(defaultVal)) {}
};

struct MethodDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<std::unique_ptr<ParamNode>> params;
    TypeNodePtr returnType;
    std::unique_ptr<BlockStmt> body;

    MethodDeclNode(std::string name, SourceLocation loc = {})
        : ASTNode(NodeKind::MethodDecl, loc), name(std::move(name)) {}
};

struct ConstructorDeclNode : ASTNode {
    std::vector<std::unique_ptr<ParamNode>> params;
    std::unique_ptr<BlockStmt> body;

    ConstructorDeclNode(SourceLocation loc = {})
        : ASTNode(NodeKind::ConstructorDecl, loc) {}
};

struct OperatorDeclNode : ASTNode {
    std::string op;
    std::vector<std::unique_ptr<ParamNode>> params;
    TypeNodePtr returnType;
    std::unique_ptr<BlockStmt> body;

    OperatorDeclNode(std::string op, SourceLocation loc = {})
        : ASTNode(NodeKind::OperatorDecl, loc), op(std::move(op)) {}
};

struct TypeDeclStmt : Statement {
    std::string name;
    std::vector<std::string> genericParams;
    std::vector<std::unique_ptr<FieldDeclNode>> fields;
    std::vector<std::unique_ptr<MethodDeclNode>> methods;
    std::vector<std::unique_ptr<OperatorDeclNode>> operators;
    bool isExported = false;

    TypeDeclStmt(std::string name, SourceLocation loc = {})
        : Statement(NodeKind::TypeDecl, loc), name(std::move(name)) {}
};

struct ClassDeclStmt : Statement {
    std::string name;
    std::vector<std::string> genericParams;
    std::string baseName;                                // extends X
    std::vector<TypeNodePtr> baseTypeArgs;
    std::vector<std::unique_ptr<FieldDeclNode>> fields;
    std::unique_ptr<ConstructorDeclNode> constructor;
    std::vector<std::unique_ptr<MethodDeclNode>> methods;
    std::vector<std::unique_ptr<OperatorDeclNode>> operators;
    bool isExported = false;

    ClassDeclStmt(std::string name, SourceLocation loc = {})
        : Statement(NodeKind::ClassDecl, loc), name(std::move(name)) {}
};

// ============================================================================
// Enums
// ============================================================================

struct EnumMemberNode : ASTNode {
    std::string name;
    ExprPtr value;

    EnumMemberNode(std::string name, ExprPtr value = nullptr, SourceLocation loc = {})
        : ASTNode(NodeKind::EnumMember, loc), name(std::move(name)),
          value(std::move(value)) {}
};

struct EnumDeclStmt : Statement {
    std::string name;
    std::vector<std::unique_ptr<EnumMemberNode>> members;
    bool isExported = false;

    EnumDeclStmt(std::string name, SourceLocation loc = {})
        : Statement(NodeKind::EnumDecl, loc), name(std::move(name)) {}
};

// ============================================================================
// Imports / Exports
// ============================================================================

struct ImportDeclStmt : Statement {
    std::vector<std::string> namedImports;  // { Point, Vector2 }
    std::string namespaceAlias;             // * as math → "math"
    std::string source;                     // "./geometry"

    ImportDeclStmt(SourceLocation loc = {})
        : Statement(NodeKind::ImportDecl, loc) {}
};

struct ExportDeclStmt : Statement {
    StmtPtr declaration;

    ExportDeclStmt(StmtPtr decl, SourceLocation loc = {})
        : Statement(NodeKind::ExportDecl, loc), declaration(std::move(decl)) {}
};

// ============================================================================
// C++ Interop
// ============================================================================

struct CppBlockStmt : Statement {
    std::string code;
    CppBlockStmt(std::string code, SourceLocation loc = {})
        : Statement(NodeKind::CppBlock, loc), code(std::move(code)) {}
};

struct CppIncludeStmt : Statement {
    std::string header;
    CppIncludeStmt(std::string header, SourceLocation loc = {})
        : Statement(NodeKind::CppInclude, loc), header(std::move(header)) {}
};

// ============================================================================
// Program Root
// ============================================================================

struct ProgramNode : ASTNode {
    std::vector<StmtPtr> statements;

    ProgramNode(SourceLocation loc = {})
        : ASTNode(NodeKind::Program, loc) {}
};

} // namespace jspp
