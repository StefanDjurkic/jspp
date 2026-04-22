#pragma once

#include "../parser/ast.hpp"
#include "../common/error.hpp"
#include "cpp_emitter.hpp"

#include <unordered_map>
#include <vector>
#include <utility>

namespace jspp {

class CodeGenerator {
public:
    CodeGenerator(ErrorReporter& errors);

    std::string generate(const ProgramNode& program);

private:
    ErrorReporter& errors_;
    CppEmitter emitter_;
    int anonStructCounter_ = 0;

    // Variable-name -> inferred element struct type for `let xs = []`
    // followed by `xs.push({...})`. Populated in a pre-pass over the
    // whole program before emission begins.
    struct InferredArrayStruct {
        std::string structName;
        std::vector<std::pair<std::string, std::string>> fields; // name, cppType
    };
    std::unordered_map<std::string, InferredArrayStruct> inferredArrayStructs_;

    // Top-level
    void emitStatement(const Statement& stmt);
    void emitVariableDecl(const VariableDeclStmt& decl);
    void emitFunctionDecl(const FunctionDeclStmt& decl);
    void emitTypeDecl(const TypeDeclStmt& decl);
    void emitClassDecl(const ClassDeclStmt& decl);
    void emitEnumDecl(const EnumDeclStmt& decl);
    void emitBlock(const BlockStmt& block);
    void emitImport(const ImportDeclStmt& decl);
    void emitCppBlock(const CppBlockStmt& block);
    void emitCppInclude(const CppIncludeStmt& inc);

    // Statements
    void emitIf(const IfStmt_& stmt);
    void emitFor(const ForStmt_& stmt);
    void emitForOf(const ForOfStmt_& stmt);
    void emitWhile(const WhileStmt_& stmt);
    void emitDoWhile(const DoWhileStmt_& stmt);
    void emitSwitch(const SwitchStmt_& stmt);
    void emitTryCatch(const TryStmt_& stmt);
    void emitReturn(const ReturnStmt_& stmt);
    void emitThrow(const ThrowStmt_& stmt);

    // Expressions
    std::string emitExpression(const Expression& expr);
    std::string emitBinaryExpr(const BinaryExpr& expr);
    std::string emitUnaryExpr(const UnaryExpr& expr);
    std::string emitCallExpr(const CallExpr& expr);
    std::string emitMemberExpr(const MemberExpr& expr);
    std::string emitIndexExpr(const IndexExpr& expr);
    std::string emitArrayLiteral(const ArrayLiteralExpr& expr);
    std::string emitObjectLiteral(const ObjectLiteralExpr& expr);
    std::string emitTemplateLiteral(const TemplateLiteralExpr& expr);
    std::string emitFunctionExpr(const FunctionExprNode& expr);
    std::string emitArrowFunction(const ArrowFunctionExpr& expr);

    // Types
    std::string emitType(const TypeNode& type);
    std::string mapPrimitiveType(const std::string& jsppType);

    // Helpers
    std::string generateAnonStructName();
    bool isStringConcatExpr(const Expression& expr);
};

} // namespace jspp
