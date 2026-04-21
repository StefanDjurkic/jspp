#pragma once

#include <vector>
#include <memory>
#include "ast.hpp"
#include "../lexer/token.hpp"
#include "../common/error.hpp"

namespace jspp {

class Parser {
public:
    Parser(const std::vector<Token>& tokens, ErrorReporter& errors);

    std::unique_ptr<ProgramNode> parse();

private:
    const std::vector<Token>& tokens_;
    ErrorReporter& errors_;
    int pos_ = 0;

    // Token navigation
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& message);
    bool isAtEnd() const;

    // Top-level declarations
    StmtPtr parseTopLevel();
    StmtPtr parseImport();
    StmtPtr parseExport();
    StmtPtr parseFunction();
    StmtPtr parseVariable();
    StmtPtr parseTypeDecl();
    StmtPtr parseClassDecl();
    StmtPtr parseEnumDecl();
    StmtPtr parseCppBlock();
    StmtPtr parseCppInclude();

    // Statements
    StmtPtr parseStatement();
    std::unique_ptr<BlockStmt> parseBlock();
    StmtPtr parseIf();
    StmtPtr parseFor();
    StmtPtr parseWhile();
    StmtPtr parseDoWhile();
    StmtPtr parseSingleOrBlock();  // Block or single statement
    StmtPtr parseSwitch();
    StmtPtr parseTryCatch();
    StmtPtr parseReturn();
    StmtPtr parseThrow();

    // Expressions (Pratt parser)
    ExprPtr parseExpression(int minPrecedence = 0);
    ExprPtr parsePrimary();
    ExprPtr parseFunctionExpr();
    ExprPtr parsePostfix(ExprPtr left);
    int getPrecedence(TokenType type) const;

    // Types
    TypeNodePtr parseTypeAnnotation();
    TypeNodePtr parseTypeExpr();

    // Helpers
    std::vector<std::unique_ptr<ParamNode>> parseParamList();
    std::vector<std::string> parseGenericParams();
};

} // namespace jspp
