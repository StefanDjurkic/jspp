#pragma once

#include "../parser/ast.hpp"
#include "../common/error.hpp"
#include "scope.hpp"
#include "types.hpp"

namespace jspp {

class TypeChecker {
public:
    TypeChecker(ErrorReporter& errors);

    void check(ProgramNode& program);

private:
    ErrorReporter& errors_;
    std::shared_ptr<Scope> currentScope_;

    // Declaration checking
    void checkStatement(Statement& stmt);
    void checkVariableDecl(VariableDeclStmt& decl);
    void checkFunctionDecl(FunctionDeclStmt& decl);
    void checkTypeDecl(TypeDeclStmt& decl);
    void checkClassDecl(ClassDeclStmt& decl);
    void checkEnumDecl(EnumDeclStmt& decl);
    void checkBlock(BlockStmt& block);
    void checkIf(IfStmt_& stmt);
    void checkFor(ForStmt_& stmt);
    void checkForOf(ForOfStmt_& stmt);
    void checkWhile(WhileStmt_& stmt);
    void checkReturn(ReturnStmt_& stmt);
    void checkTryCatch(TryStmt_& stmt);

    // Expression type inference
    std::shared_ptr<ResolvedType> inferType(Expression& expr);
    std::shared_ptr<ResolvedType> inferBinary(BinaryExpr& expr);
    std::shared_ptr<ResolvedType> inferUnary(UnaryExpr& expr);
    std::shared_ptr<ResolvedType> inferCall(CallExpr& expr);
    std::shared_ptr<ResolvedType> inferMember(MemberExpr& expr);
    std::shared_ptr<ResolvedType> inferIndex(IndexExpr& expr);

    // Type resolution
    std::shared_ptr<ResolvedType> resolveTypeNode(TypeNode& type);
    bool isAssignable(const ResolvedType& target, const ResolvedType& source);

    // Scope management
    void pushScope();
    void popScope();
};

} // namespace jspp
