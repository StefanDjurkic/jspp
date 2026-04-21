#include "parser.hpp"
#include "../lexer/lexer.hpp"
#include <stdexcept>
#include <algorithm>

namespace jspp {

// ============================================================================
// Constructor
// ============================================================================

Parser::Parser(const std::vector<Token>& tokens, ErrorReporter& errors)
    : tokens_(tokens), errors_(errors) {}

// ============================================================================
// Token navigation
// ============================================================================

const Token& Parser::current() const {
    if (pos_ >= static_cast<int>(tokens_.size()))
        return tokens_.back();
    return tokens_[pos_];
}

const Token& Parser::peek(int offset) const {
    int idx = pos_ + offset;
    if (idx < 0 || idx >= static_cast<int>(tokens_.size()))
        return tokens_.back();
    return tokens_[idx];
}

const Token& Parser::advance() {
    const Token& tok = current();
    if (pos_ < static_cast<int>(tokens_.size())) pos_++;
    return tok;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    auto& tok = current();
    errors_.error("E100", message + " (got " + tokenTypeName(tok.type) + " '" + tok.value + "')", tok.loc);
    throw std::runtime_error("Parse error: " + message);
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::TOK_EOF;
}

// ============================================================================
// Helpers
// ============================================================================

static bool isTypeKeyword(TokenType t) {
    return t == TokenType::TYPE_INT  || t == TokenType::TYPE_I8  ||
           t == TokenType::TYPE_I16  || t == TokenType::TYPE_I32 ||
           t == TokenType::TYPE_I64  || t == TokenType::TYPE_U8  ||
           t == TokenType::TYPE_U16  || t == TokenType::TYPE_U32 ||
           t == TokenType::TYPE_U64  || t == TokenType::TYPE_FLOAT ||
           t == TokenType::TYPE_DOUBLE || t == TokenType::TYPE_BOOL ||
           t == TokenType::TYPE_STRING || t == TokenType::TYPE_CHAR ||
           t == TokenType::TYPE_VOID || t == TokenType::TYPE_AUTO ||
           t == TokenType::TYPE_SIZE || t == TokenType::TYPE_PTR;
}

// ============================================================================
// Program entry
// ============================================================================

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>(current().loc);
    while (!isAtEnd()) {
        program->statements.push_back(parseTopLevel());
    }
    return program;
}

// ============================================================================
// Top-level declarations
// ============================================================================

StmtPtr Parser::parseTopLevel() {
    auto& tok = current();
    switch (tok.type) {
        case TokenType::KW_FUNCTION:    return parseFunction();
        case TokenType::KW_LET:
        case TokenType::KW_CONST:       return parseVariable();
        case TokenType::KW_CLASS:       return parseClassDecl();
        case TokenType::KW_TYPE:        return parseTypeDecl();
        case TokenType::KW_ENUM:        return parseEnumDecl();
        case TokenType::KW_IMPORT:      return parseImport();
        case TokenType::KW_EXPORT:      return parseExport();
        case TokenType::KW_CPP_INCLUDE: return parseCppInclude();
        case TokenType::KW_CPP:         return parseCppBlock();
        default:                        return parseStatement();
    }
}

// ============================================================================
// Statements
// ============================================================================

StmtPtr Parser::parseStatement() {
    auto& tok = current();
    switch (tok.type) {
        case TokenType::KW_LET:
        case TokenType::KW_CONST:       return parseVariable();
        case TokenType::KW_RETURN:      return parseReturn();
        case TokenType::KW_IF:          return parseIf();
        case TokenType::KW_FOR:         return parseFor();
        case TokenType::KW_WHILE:       return parseWhile();
        case TokenType::KW_DO:          return parseDoWhile();
        case TokenType::KW_SWITCH:      return parseSwitch();
        case TokenType::KW_TRY:         return parseTryCatch();
        case TokenType::KW_THROW:       return parseThrow();
        case TokenType::KW_BREAK:
            advance();
            expect(TokenType::DL_SEMICOLON, "Expected ';' after break");
            return std::make_unique<BreakStmt_>(tok.loc);
        case TokenType::KW_CONTINUE:
            advance();
            expect(TokenType::DL_SEMICOLON, "Expected ';' after continue");
            return std::make_unique<ContinueStmt_>(tok.loc);
        case TokenType::DL_LBRACE:
            return parseBlock();
        case TokenType::KW_FUNCTION:
            return parseFunction();
        default: {
            // Expression statement
            auto loc = tok.loc;
            auto expr = parseExpression();
            expect(TokenType::DL_SEMICOLON, "Expected ';' after expression");
            return std::make_unique<ExpressionStmt>(std::move(expr), loc);
        }
    }
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    auto loc = current().loc;
    expect(TokenType::DL_LBRACE, "Expected '{'");
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
        stmts.push_back(parseStatement());
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    return std::make_unique<BlockStmt>(std::move(stmts), loc);
}

StmtPtr Parser::parseSingleOrBlock() {
    if (check(TokenType::DL_LBRACE)) return parseBlock();
    auto loc = current().loc;
    auto stmt = parseStatement();
    std::vector<StmtPtr> stmts;
    stmts.push_back(std::move(stmt));
    return std::make_unique<BlockStmt>(std::move(stmts), loc);
}

// ============================================================================
// Variable declaration
// ============================================================================

StmtPtr Parser::parseVariable() {
    auto loc = current().loc;
    bool isConst = (current().type == TokenType::KW_CONST);
    advance(); // let / const

    auto nameToken = expect(TokenType::IDENTIFIER, "Expected variable name");
    std::string name = nameToken.value;

    TypeNodePtr typeAnnotation;
    if (match(TokenType::OP_COLON)) {
        typeAnnotation = parseTypeExpr();
    }

    ExprPtr init;
    if (match(TokenType::OP_ASSIGN)) {
        init = parseExpression();
    }

    expect(TokenType::DL_SEMICOLON, "Expected ';' after variable declaration");
    return std::make_unique<VariableDeclStmt>(std::move(name), isConst,
                                              std::move(typeAnnotation), std::move(init), loc);
}

// ============================================================================
// Function declaration
// ============================================================================

StmtPtr Parser::parseFunction() {
    auto loc = current().loc;
    expect(TokenType::KW_FUNCTION, "Expected 'function'");
    auto nameToken = expect(TokenType::IDENTIFIER, "Expected function name");

    auto func = std::make_unique<FunctionDeclStmt>(nameToken.value, loc);
    func->genericParams = parseGenericParams();
    expect(TokenType::DL_LPAREN, "Expected '('");
    func->params = parseParamList();
    expect(TokenType::DL_RPAREN, "Expected ')'");

    if (match(TokenType::OP_COLON)) {
        func->returnType = parseTypeExpr();
    }
    func->body = parseBlock();
    return func;
}

std::vector<std::unique_ptr<ParamNode>> Parser::parseParamList() {
    std::vector<std::unique_ptr<ParamNode>> params;
    while (!check(TokenType::DL_RPAREN) && !isAtEnd()) {
        bool isRef = match(TokenType::KW_REF);
        auto nameTok = expect(TokenType::IDENTIFIER, "Expected parameter name");

        TypeNodePtr typeAnnotation;
        if (match(TokenType::OP_COLON)) {
            typeAnnotation = parseTypeExpr();
        }

        ExprPtr defaultValue;
        if (match(TokenType::OP_ASSIGN)) {
            defaultValue = parseExpression();
        }

        params.push_back(std::make_unique<ParamNode>(
            nameTok.value, std::move(typeAnnotation), std::move(defaultValue), isRef, nameTok.loc));

        if (!check(TokenType::DL_RPAREN)) {
            expect(TokenType::DL_COMMA, "Expected ',' between parameters");
        }
    }
    return params;
}

std::vector<std::string> Parser::parseGenericParams() {
    if (!check(TokenType::OP_LT)) return {};

    int saved = pos_;
    advance(); // <
    std::vector<std::string> params;

    if (check(TokenType::IDENTIFIER)) {
        params.push_back(advance().value);
        while (match(TokenType::DL_COMMA)) {
            if (check(TokenType::IDENTIFIER)) {
                params.push_back(advance().value);
            } else {
                pos_ = saved;
                return {};
            }
        }
        if (match(TokenType::OP_GT)) return params;
    }

    pos_ = saved;
    return {};
}

// ============================================================================
// Type expressions
// ============================================================================

TypeNodePtr Parser::parseTypeAnnotation() {
    return parseTypeExpr();
}

TypeNodePtr Parser::parseTypeExpr() {
    std::string name;
    auto loc = current().loc;

    if (isTypeKeyword(current().type) || current().type == TokenType::IDENTIFIER) {
        name = advance().value;
    } else {
        errors_.error("E101", "Expected type", current().loc);
        throw std::runtime_error("Expected type");
    }

    // Check for array suffix: T[]
    bool isArray = false;
    if (check(TokenType::DL_LBRACKET) && peek().type == TokenType::DL_RBRACKET) {
        advance(); advance();
        isArray = true;
    }

    // Optional suffix: T?
    bool isOptional = false;
    if (match(TokenType::OP_QUESTION)) {
        isOptional = true;
    }

    if (isArray) {
        auto elemType = TypeNode::makePrimitive(name, loc);
        auto arrayType = TypeNode::makeArray(std::move(elemType), loc);
        if (isOptional) {
            return TypeNode::makeOptional(std::move(arrayType), loc);
        }
        return arrayType;
    }

    if (isOptional) {
        auto inner = TypeNode::makePrimitive(name, loc);
        return TypeNode::makeOptional(std::move(inner), loc);
    }

    // Check if it's a known type keyword or treat as named/primitive
    if (isTypeKeyword(tokens_[pos_ - 1].type)) {
        return TypeNode::makePrimitive(name, loc);
    }
    return TypeNode::makeNamed(name, loc);
}

// ============================================================================
// Control flow
// ============================================================================

StmtPtr Parser::parseReturn() {
    auto loc = current().loc;
    advance(); // return
    ExprPtr value;
    if (!check(TokenType::DL_SEMICOLON)) {
        value = parseExpression();
    }
    expect(TokenType::DL_SEMICOLON, "Expected ';' after return");
    return std::make_unique<ReturnStmt_>(std::move(value), loc);
}

StmtPtr Parser::parseIf() {
    auto loc = current().loc;
    advance(); // if
    expect(TokenType::DL_LPAREN, "Expected '('");
    auto condition = parseExpression();
    expect(TokenType::DL_RPAREN, "Expected ')'");

    auto ifStmt = std::make_unique<IfStmt_>(loc);
    ifStmt->condition = std::move(condition);

    // Then block
    if (check(TokenType::DL_LBRACE)) {
        ifStmt->thenBlock = parseBlock();
    } else {
        auto stmtLoc = current().loc;
        auto s = parseStatement();
        std::vector<StmtPtr> stmts;
        stmts.push_back(std::move(s));
        ifStmt->thenBlock = std::make_unique<BlockStmt>(std::move(stmts), stmtLoc);
    }

    // Else
    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_IF)) {
            // else if
            auto elseIfStmt = parseIf();
            // Package as block containing the nested if
            std::vector<StmtPtr> v;
            auto elseIfLoc = elseIfStmt->loc;
            v.push_back(std::move(elseIfStmt));
            ifStmt->elseBlock = std::make_unique<BlockStmt>(std::move(v), elseIfLoc);
        } else if (check(TokenType::DL_LBRACE)) {
            ifStmt->elseBlock = parseBlock();
        } else {
            auto stmtLoc = current().loc;
            auto s = parseStatement();
            std::vector<StmtPtr> stmts;
            stmts.push_back(std::move(s));
            ifStmt->elseBlock = std::make_unique<BlockStmt>(std::move(stmts), stmtLoc);
        }
    }

    return ifStmt;
}

StmtPtr Parser::parseFor() {
    auto loc = current().loc;
    advance(); // for
    expect(TokenType::DL_LPAREN, "Expected '('");

    // Check for for-of: for (let x of iterable)
    if ((check(TokenType::KW_LET) || check(TokenType::KW_CONST)) &&
        peek(1).type == TokenType::IDENTIFIER &&
        peek(2).type == TokenType::KW_OF) {

        bool isConst = (current().type == TokenType::KW_CONST);
        advance(); // let/const
        std::string varName = advance().value;
        advance(); // of
        auto iterable = parseExpression();
        expect(TokenType::DL_RPAREN, "Expected ')'");

        auto forOf = std::make_unique<ForOfStmt_>(loc);
        forOf->varName = varName;
        forOf->isConst = isConst;
        forOf->iterable = std::move(iterable);
        if (check(TokenType::DL_LBRACE)) {
            forOf->body = parseBlock();
        } else {
            auto stmtLoc = current().loc;
            auto s = parseStatement();
            std::vector<StmtPtr> stmts;
            stmts.push_back(std::move(s));
            forOf->body = std::make_unique<BlockStmt>(std::move(stmts), stmtLoc);
        }
        return forOf;
    }

    // C-style for
    auto forStmt = std::make_unique<ForStmt_>(loc);

    // Init
    if (!check(TokenType::DL_SEMICOLON)) {
        if (check(TokenType::KW_LET) || check(TokenType::KW_CONST)) {
            forStmt->init = parseVariable();  // eats the semicolon
            // But we already ate the semicolon in parseVariable, so don't eat another
            goto parse_condition;
        } else {
            auto expr = parseExpression();
            forStmt->init = std::make_unique<ExpressionStmt>(std::move(expr), loc);
        }
    }
    expect(TokenType::DL_SEMICOLON, "Expected ';' in for");

parse_condition:
    if (!check(TokenType::DL_SEMICOLON)) {
        forStmt->condition = parseExpression();
    }
    expect(TokenType::DL_SEMICOLON, "Expected ';' in for");

    // Update
    if (!check(TokenType::DL_RPAREN)) {
        forStmt->update = parseExpression();
    }
    expect(TokenType::DL_RPAREN, "Expected ')'");

    if (check(TokenType::DL_LBRACE)) {
        forStmt->body = parseBlock();
    } else {
        auto stmtLoc = current().loc;
        auto s = parseStatement();
        std::vector<StmtPtr> stmts;
        stmts.push_back(std::move(s));
        forStmt->body = std::make_unique<BlockStmt>(std::move(stmts), stmtLoc);
    }
    return forStmt;
}

StmtPtr Parser::parseWhile() {
    auto loc = current().loc;
    advance(); // while
    expect(TokenType::DL_LPAREN, "Expected '('");
    auto condition = parseExpression();
    expect(TokenType::DL_RPAREN, "Expected ')'");

    auto ws = std::make_unique<WhileStmt_>(loc);
    ws->condition = std::move(condition);
    if (check(TokenType::DL_LBRACE)) {
        ws->body = parseBlock();
    } else {
        auto stmtLoc = current().loc;
        auto s = parseStatement();
        std::vector<StmtPtr> stmts;
        stmts.push_back(std::move(s));
        ws->body = std::make_unique<BlockStmt>(std::move(stmts), stmtLoc);
    }
    return ws;
}

StmtPtr Parser::parseDoWhile() {
    auto loc = current().loc;
    advance(); // do
    auto body = parseBlock();
    expect(TokenType::KW_WHILE, "Expected 'while'");
    expect(TokenType::DL_LPAREN, "Expected '('");
    auto condition = parseExpression();
    expect(TokenType::DL_RPAREN, "Expected ')'");
    expect(TokenType::DL_SEMICOLON, "Expected ';'");

    auto dw = std::make_unique<DoWhileStmt_>(loc);
    dw->condition = std::move(condition);
    dw->body = std::move(body);
    return dw;
}

StmtPtr Parser::parseSwitch() {
    auto loc = current().loc;
    advance(); // switch
    expect(TokenType::DL_LPAREN, "Expected '('");
    auto disc = parseExpression();
    expect(TokenType::DL_RPAREN, "Expected ')'");
    expect(TokenType::DL_LBRACE, "Expected '{'");

    auto sw = std::make_unique<SwitchStmt_>(loc);
    sw->discriminant = std::move(disc);

    while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
        if (match(TokenType::KW_CASE)) {
            auto caseLoc = current().loc;
            auto val = parseExpression();
            expect(TokenType::OP_COLON, "Expected ':'");
            std::vector<StmtPtr> body;
            while (!check(TokenType::KW_CASE) && !check(TokenType::KW_DEFAULT) &&
                   !check(TokenType::DL_RBRACE)) {
                body.push_back(parseStatement());
            }
            sw->cases.push_back(std::make_unique<CaseClauseNode>(
                std::move(val), std::move(body), caseLoc));
        } else if (match(TokenType::KW_DEFAULT)) {
            auto defLoc = current().loc;
            expect(TokenType::OP_COLON, "Expected ':'");
            std::vector<StmtPtr> body;
            while (!check(TokenType::KW_CASE) && !check(TokenType::DL_RBRACE)) {
                body.push_back(parseStatement());
            }
            sw->defaultCase = std::make_unique<DefaultClauseNode>(std::move(body), defLoc);
        }
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    return sw;
}

StmtPtr Parser::parseTryCatch() {
    auto loc = current().loc;
    advance(); // try
    auto tryBlock = parseBlock();
    expect(TokenType::KW_CATCH, "Expected 'catch'");
    expect(TokenType::DL_LPAREN, "Expected '('");
    auto paramTok = expect(TokenType::IDENTIFIER, "Expected catch parameter name");

    auto tryStmt = std::make_unique<TryStmt_>(loc);
    tryStmt->tryBlock = std::move(tryBlock);

    auto catchClause = std::make_unique<CatchClauseNode>(paramTok.loc);
    catchClause->paramName = paramTok.value;
    if (match(TokenType::OP_COLON)) {
        catchClause->paramType = parseTypeExpr();
    }
    expect(TokenType::DL_RPAREN, "Expected ')'");
    catchClause->body = parseBlock();

    tryStmt->catchClause = std::move(catchClause);
    return tryStmt;
}

StmtPtr Parser::parseThrow() {
    auto loc = current().loc;
    advance(); // throw
    auto value = parseExpression();
    expect(TokenType::DL_SEMICOLON, "Expected ';' after throw");
    return std::make_unique<ThrowStmt_>(std::move(value), loc);
}

// ============================================================================
// Class / Type / Enum declarations
// ============================================================================

StmtPtr Parser::parseClassDecl() {
    auto loc = current().loc;
    advance(); // class
    auto nameTok = expect(TokenType::IDENTIFIER, "Expected class name");
    auto cls = std::make_unique<ClassDeclStmt>(nameTok.value, loc);

    if (match(TokenType::KW_EXTENDS)) {
        cls->baseName = expect(TokenType::IDENTIFIER, "Expected base class name").value;
    }

    expect(TokenType::DL_LBRACE, "Expected '{'");

    while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
        // Constructor
        if (current().value == "constructor") {
            advance();
            expect(TokenType::DL_LPAREN, "Expected '('");
            auto ctor = std::make_unique<ConstructorDeclNode>(current().loc);
            ctor->params = parseParamList();
            expect(TokenType::DL_RPAREN, "Expected ')'");
            ctor->body = parseBlock();
            cls->constructor = std::move(ctor);
        }
        // Method
        else if (check(TokenType::KW_FUNCTION)) {
            auto funcStmt = parseFunction();
            // Convert FunctionDeclStmt to MethodDeclNode
            auto* fd = static_cast<FunctionDeclStmt*>(funcStmt.get());
            auto method = std::make_unique<MethodDeclNode>(fd->name, fd->loc);
            method->genericParams = std::move(fd->genericParams);
            method->params = std::move(fd->params);
            method->returnType = std::move(fd->returnType);
            method->body = std::move(fd->body);
            cls->methods.push_back(std::move(method));
        }
        // Field
        else if (check(TokenType::IDENTIFIER) || isTypeKeyword(current().type)) {
            auto fnameTok = advance();
            TypeNodePtr ftype;
            if (match(TokenType::OP_COLON)) {
                ftype = parseTypeExpr();
            }
            ExprPtr defaultVal;
            if (match(TokenType::OP_ASSIGN)) {
                defaultVal = parseExpression();
            }
            expect(TokenType::DL_SEMICOLON, "Expected ';' after field");
            cls->fields.push_back(std::make_unique<FieldDeclNode>(
                fnameTok.value, std::move(ftype), std::move(defaultVal), fnameTok.loc));
        } else {
            errors_.error("E102", "Unexpected token in class body: " + current().value, current().loc);
            advance();
        }
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    return cls;
}

StmtPtr Parser::parseTypeDecl() {
    auto loc = current().loc;
    advance(); // type
    auto nameTok = expect(TokenType::IDENTIFIER, "Expected type name");
    auto typeDecl = std::make_unique<TypeDeclStmt>(nameTok.value, loc);
    typeDecl->genericParams = parseGenericParams();

    expect(TokenType::OP_ASSIGN, "Expected '='");
    expect(TokenType::DL_LBRACE, "Expected '{'");

    while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
        if (check(TokenType::KW_FUNCTION)) {
            auto funcStmt = parseFunction();
            auto* fd = static_cast<FunctionDeclStmt*>(funcStmt.get());
            auto method = std::make_unique<MethodDeclNode>(fd->name, fd->loc);
            method->genericParams = std::move(fd->genericParams);
            method->params = std::move(fd->params);
            method->returnType = std::move(fd->returnType);
            method->body = std::move(fd->body);
            typeDecl->methods.push_back(std::move(method));
        } else {
            auto fnameTok = expect(TokenType::IDENTIFIER, "Expected field name");
            expect(TokenType::OP_COLON, "Expected ':'");
            auto ftype = parseTypeExpr();
            ExprPtr defaultVal;
            if (match(TokenType::OP_ASSIGN)) {
                defaultVal = parseExpression();
            }
            expect(TokenType::DL_SEMICOLON, "Expected ';'");
            typeDecl->fields.push_back(std::make_unique<FieldDeclNode>(
                fnameTok.value, std::move(ftype), std::move(defaultVal), fnameTok.loc));
        }
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    expect(TokenType::DL_SEMICOLON, "Expected ';'");
    return typeDecl;
}

StmtPtr Parser::parseEnumDecl() {
    auto loc = current().loc;
    advance(); // enum
    auto nameTok = expect(TokenType::IDENTIFIER, "Expected enum name");
    auto enumDecl = std::make_unique<EnumDeclStmt>(nameTok.value, loc);

    expect(TokenType::DL_LBRACE, "Expected '{'");
    while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
        auto memberTok = expect(TokenType::IDENTIFIER, "Expected enum member name");
        ExprPtr val;
        if (match(TokenType::OP_ASSIGN)) {
            val = parseExpression();
        }
        enumDecl->members.push_back(std::make_unique<EnumMemberNode>(
            memberTok.value, std::move(val), memberTok.loc));
        match(TokenType::DL_COMMA); // optional comma
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    return enumDecl;
}

// ============================================================================
// Import / Export / Cpp interop
// ============================================================================

StmtPtr Parser::parseImport() {
    auto loc = current().loc;
    advance(); // import
    auto importDecl = std::make_unique<ImportDeclStmt>(loc);

    if (match(TokenType::OP_STAR)) {
        expect(TokenType::KW_AS, "Expected 'as'");
        importDecl->namespaceAlias = expect(TokenType::IDENTIFIER, "Expected alias name").value;
    } else {
        expect(TokenType::DL_LBRACE, "Expected '{'");
        while (!check(TokenType::DL_RBRACE)) {
            importDecl->namedImports.push_back(
                expect(TokenType::IDENTIFIER, "Expected import name").value);
            if (!check(TokenType::DL_RBRACE)) {
                expect(TokenType::DL_COMMA, "Expected ','");
            }
        }
        expect(TokenType::DL_RBRACE, "Expected '}'");
    }

    expect(TokenType::KW_FROM, "Expected 'from'");
    importDecl->source = expect(TokenType::LIT_STRING, "Expected module path").value;
    expect(TokenType::DL_SEMICOLON, "Expected ';'");
    return importDecl;
}

StmtPtr Parser::parseExport() {
    auto loc = current().loc;
    advance(); // export
    auto decl = parseTopLevel();
    return std::make_unique<ExportDeclStmt>(std::move(decl), loc);
}

StmtPtr Parser::parseCppInclude() {
    auto loc = current().loc;
    advance(); // cpp_include
    auto header = expect(TokenType::LIT_STRING, "Expected header string").value;
    expect(TokenType::DL_SEMICOLON, "Expected ';'");
    return std::make_unique<CppIncludeStmt>(std::move(header), loc);
}

StmtPtr Parser::parseCppBlock() {
    auto loc = current().loc;
    advance(); // cpp
    expect(TokenType::DL_LBRACE, "Expected '{'");

    int depth = 1;
    std::string code;
    while (depth > 0 && !isAtEnd()) {
        if (check(TokenType::DL_LBRACE)) depth++;
        if (check(TokenType::DL_RBRACE)) {
            depth--;
            if (depth == 0) break;
        }
        code += current().value + " ";
        advance();
    }
    expect(TokenType::DL_RBRACE, "Expected '}'");
    // trim trailing space
    while (!code.empty() && code.back() == ' ') code.pop_back();
    return std::make_unique<CppBlockStmt>(std::move(code), loc);
}

// ============================================================================
// Expression parsing — Precedence climbing
// ============================================================================

int Parser::getPrecedence(TokenType type) const {
    switch (type) {
        case TokenType::OP_ASSIGN:
        case TokenType::OP_PLUS_ASSIGN:
        case TokenType::OP_MINUS_ASSIGN:
        case TokenType::OP_STAR_ASSIGN:
        case TokenType::OP_SLASH_ASSIGN:
        case TokenType::OP_PERCENT_ASSIGN:
            return 1;
        case TokenType::OP_QUESTION:        return 2;  // ternary
        case TokenType::OP_OR:              return 3;
        case TokenType::OP_AND:             return 4;
        case TokenType::OP_BIT_OR:          return 5;
        case TokenType::OP_BIT_XOR:         return 6;
        case TokenType::OP_BIT_AND:         return 7;
        case TokenType::OP_STRICT_EQ:
        case TokenType::OP_STRICT_NEQ:      return 8;
        case TokenType::OP_LT:
        case TokenType::OP_GT:
        case TokenType::OP_LTE:
        case TokenType::OP_GTE:             return 9;
        case TokenType::OP_LSHIFT:
        case TokenType::OP_RSHIFT:          return 10;
        case TokenType::OP_PLUS:
        case TokenType::OP_MINUS:           return 11;
        case TokenType::OP_STAR:
        case TokenType::OP_SLASH:
        case TokenType::OP_PERCENT:         return 12;
        default:                            return 0;
    }
}

ExprPtr Parser::parseExpression(int minPrecedence) {
    auto left = parsePrimary();
    left = parsePostfix(std::move(left));

    while (true) {
        auto& tok = current();
        int prec = getPrecedence(tok.type);
        if (prec < minPrecedence || prec == 0) break;

        // Assignment operators (right-associative)
        if (tok.type == TokenType::OP_ASSIGN || tok.type == TokenType::OP_PLUS_ASSIGN ||
            tok.type == TokenType::OP_MINUS_ASSIGN || tok.type == TokenType::OP_STAR_ASSIGN ||
            tok.type == TokenType::OP_SLASH_ASSIGN || tok.type == TokenType::OP_PERCENT_ASSIGN) {
            auto opTok = advance();
            auto right = parseExpression(prec); // right-assoc: same precedence
            left = std::make_unique<AssignExpr>(opTok.value, std::move(left), std::move(right), opTok.loc);
            continue;
        }

        // Ternary
        if (tok.type == TokenType::OP_QUESTION) {
            advance();
            auto thenExpr = parseExpression();
            expect(TokenType::OP_COLON, "Expected ':' in ternary");
            auto elseExpr = parseExpression(prec);
            left = std::make_unique<TernaryExpr>(std::move(left), std::move(thenExpr), std::move(elseExpr), tok.loc);
            continue;
        }

        // Binary operators (left-associative)
        auto opTok = advance();
        auto right = parseExpression(prec + 1); // left-assoc: higher precedence
        right = parsePostfix(std::move(right));
        left = std::make_unique<BinaryExpr>(opTok.value, std::move(left), std::move(right), opTok.loc);
    }

    return left;
}

ExprPtr Parser::parsePrimary() {
    auto& tok = current();
    auto loc = tok.loc;

    // Literals
    if (tok.type == TokenType::LIT_INT) {
        advance();
        int64_t val = 0;
        if (tok.value.size() > 2 && tok.value[1] == 'x') {
            val = std::stoll(tok.value, nullptr, 16);
        } else if (tok.value.size() > 2 && tok.value[1] == 'b') {
            val = std::stoll(tok.value.substr(2), nullptr, 2);
        } else {
            val = std::stoll(tok.value);
        }
        return std::make_unique<IntLiteralExpr>(val, loc);
    }
    if (tok.type == TokenType::LIT_HEX) {
        advance();
        return std::make_unique<IntLiteralExpr>(std::stoll(tok.value, nullptr, 16), loc);
    }
    if (tok.type == TokenType::LIT_BIN) {
        advance();
        return std::make_unique<IntLiteralExpr>(std::stoll(tok.value.substr(2), nullptr, 2), loc);
    }
    if (tok.type == TokenType::LIT_FLOAT) {
        advance();
        return std::make_unique<FloatLiteralExpr>(std::stod(tok.value), loc);
    }
    if (tok.type == TokenType::LIT_STRING) {
        advance();
        return std::make_unique<StringLiteralExpr>(tok.value, loc);
    }
    if (tok.type == TokenType::LIT_CHAR) {
        advance();
        return std::make_unique<CharLiteralExpr>(tok.value.empty() ? '\0' : tok.value[0], loc);
    }
    if (tok.type == TokenType::LIT_TEMPLATE) {
        advance();
        // Parse template literal parts: split on ${...}
        std::vector<std::string> strings;
        std::vector<ExprPtr> expressions;
        const std::string& raw = tok.value;
        std::string cur;
        size_t i = 0;
        while (i < raw.size()) {
            if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{') {
                strings.push_back(cur);
                cur.clear();
                i += 2;
                int depth = 1;
                std::string exprStr;
                while (i < raw.size() && depth > 0) {
                    if (raw[i] == '{') depth++;
                    if (raw[i] == '}') {
                        depth--;
                        if (depth == 0) { i++; break; }
                    }
                    exprStr += raw[i];
                    i++;
                }
                // Parse the expression string
                Lexer innerLexer(exprStr, loc.filename);
                auto innerTokens = innerLexer.tokenize();
                ErrorReporter innerErrors;
                Parser innerParser(innerTokens, innerErrors);
                expressions.push_back(innerParser.parseExpression());
            } else {
                cur += raw[i];
                i++;
            }
        }
        strings.push_back(cur);
        return std::make_unique<TemplateLiteralExpr>(
            std::move(strings), std::move(expressions), loc);
    }
    if (tok.type == TokenType::KW_TRUE) {
        advance();
        return std::make_unique<BoolLiteralExpr>(true, loc);
    }
    if (tok.type == TokenType::KW_FALSE) {
        advance();
        return std::make_unique<BoolLiteralExpr>(false, loc);
    }
    if (tok.type == TokenType::KW_NULL) {
        advance();
        return std::make_unique<NullLiteralExpr>(loc);
    }
    if (tok.type == TokenType::KW_THIS) {
        advance();
        return std::make_unique<ThisExpr>(loc);
    }
    if (tok.type == TokenType::KW_SUPER) {
        advance();
        return std::make_unique<SuperExpr>(loc);
    }

    // Anonymous/named function expression
    if (tok.type == TokenType::KW_FUNCTION &&
        (peek().type == TokenType::DL_LPAREN || peek().type == TokenType::IDENTIFIER)) {
        return parseFunctionExpr();
    }

    // new expression
    if (tok.type == TokenType::KW_NEW) {
        advance();
        auto nameTok = expect(TokenType::IDENTIFIER, "Expected class name after 'new'");
        expect(TokenType::DL_LPAREN, "Expected '('");
        std::vector<ExprPtr> args;
        while (!check(TokenType::DL_RPAREN) && !isAtEnd()) {
            args.push_back(parseExpression());
            if (!check(TokenType::DL_RPAREN))
                expect(TokenType::DL_COMMA, "Expected ','");
        }
        expect(TokenType::DL_RPAREN, "Expected ')'");
        return std::make_unique<NewExprNode>(nameTok.value, std::move(args), loc);
    }

    // Unary prefix operators
    if (tok.type == TokenType::OP_NOT || tok.type == TokenType::OP_MINUS ||
        tok.type == TokenType::OP_BIT_NOT) {
        auto opTok = advance();
        auto operand = parsePrimary();
        operand = parsePostfix(std::move(operand));
        return std::make_unique<UnaryExpr>(opTok.value, std::move(operand), true, loc);
    }
    if (tok.type == TokenType::OP_INCREMENT || tok.type == TokenType::OP_DECREMENT) {
        auto opTok = advance();
        auto operand = parsePrimary();
        return std::make_unique<UnaryExpr>(opTok.value, std::move(operand), true, loc);
    }

    // Identifier
    if (tok.type == TokenType::IDENTIFIER) {
        advance();
        return std::make_unique<IdentifierExpr>(tok.value, loc);
    }

    // Parenthesized expression or arrow function
    if (tok.type == TokenType::DL_LPAREN) {
        // Try arrow function: (...) =>
        int saved = pos_;
        try {
            advance(); // (
            auto params = parseParamList();
            expect(TokenType::DL_RPAREN, "Expected ')'");

            TypeNodePtr returnType;
            if (match(TokenType::OP_COLON)) {
                returnType = parseTypeExpr();
            }
            expect(TokenType::OP_ARROW, "Expected '=>'");

            auto arrow = std::make_unique<ArrowFunctionExpr>(loc);
            arrow->params = std::move(params);
            arrow->returnType = std::move(returnType);

            if (check(TokenType::DL_LBRACE)) {
                auto block = parseBlock();
                arrow->bodyBlock = std::move(block->statements);
                arrow->hasBlockBody = true;
            } else {
                arrow->bodyExpr = parseExpression();
                arrow->hasBlockBody = false;
            }
            return arrow;
        } catch (...) {
            pos_ = saved;
            advance(); // (
            auto expr = parseExpression();
            expect(TokenType::DL_RPAREN, "Expected ')'");
            return expr;
        }
    }

    // Array literal
    if (tok.type == TokenType::DL_LBRACKET) {
        advance();
        std::vector<ExprPtr> elements;
        while (!check(TokenType::DL_RBRACKET) && !isAtEnd()) {
            elements.push_back(parseExpression());
            if (!check(TokenType::DL_RBRACKET))
                expect(TokenType::DL_COMMA, "Expected ','");
        }
        expect(TokenType::DL_RBRACKET, "Expected ']'");
        return std::make_unique<ArrayLiteralExpr>(std::move(elements), loc);
    }

    // Object literal
    if (tok.type == TokenType::DL_LBRACE) {
        advance();
        std::vector<ObjectField> fields;
        while (!check(TokenType::DL_RBRACE) && !isAtEnd()) {
            auto key = expect(TokenType::IDENTIFIER, "Expected field name").value;
            expect(TokenType::OP_COLON, "Expected ':'");
            auto val = parseExpression();
            fields.push_back({key, std::move(val), loc});
            if (!check(TokenType::DL_RBRACE))
                expect(TokenType::DL_COMMA, "Expected ','");
        }
        expect(TokenType::DL_RBRACE, "Expected '}'");
        return std::make_unique<ObjectLiteralExpr>(std::move(fields), loc);
    }

    errors_.error("E103", "Unexpected token: " + std::string(tokenTypeName(tok.type)) +
                  " '" + tok.value + "'", tok.loc);
    advance();
    return std::make_unique<IntLiteralExpr>(0, loc); // recovery
}

ExprPtr Parser::parseFunctionExpr() {
    auto loc = current().loc;
    advance(); // function
    auto funcExpr = std::make_unique<FunctionExprNode>(loc);
    if (check(TokenType::IDENTIFIER)) {
        funcExpr->name = advance().value;
    }
    expect(TokenType::DL_LPAREN, "Expected '('");
    funcExpr->params = parseParamList();
    expect(TokenType::DL_RPAREN, "Expected ')'");
    if (match(TokenType::OP_COLON)) {
        funcExpr->returnType = parseTypeExpr();
    }
    funcExpr->body = parseBlock();
    return funcExpr;
}

ExprPtr Parser::parsePostfix(ExprPtr left) {
    while (true) {
        // Postfix ++ / --
        if (check(TokenType::OP_INCREMENT)) {
            auto loc = current().loc;
            advance();
            left = std::make_unique<UnaryExpr>("++", std::move(left), false, loc);
            continue;
        }
        if (check(TokenType::OP_DECREMENT)) {
            auto loc = current().loc;
            advance();
            left = std::make_unique<UnaryExpr>("--", std::move(left), false, loc);
            continue;
        }

        // Function call
        if (check(TokenType::DL_LPAREN)) {
            auto loc = current().loc;
            advance();
            std::vector<ExprPtr> args;
            while (!check(TokenType::DL_RPAREN) && !isAtEnd()) {
                args.push_back(parseExpression());
                if (!check(TokenType::DL_RPAREN))
                    expect(TokenType::DL_COMMA, "Expected ','");
            }
            expect(TokenType::DL_RPAREN, "Expected ')'");
            left = std::make_unique<CallExpr>(std::move(left), std::move(args), loc);
            continue;
        }

        // Member access
        if (match(TokenType::OP_DOT)) {
            auto memberTok = expect(TokenType::IDENTIFIER, "Expected member name");
            left = std::make_unique<MemberExpr>(std::move(left), memberTok.value, memberTok.loc);
            continue;
        }

        // Index access
        if (check(TokenType::DL_LBRACKET)) {
            auto loc = current().loc;
            advance();
            auto index = parseExpression();
            expect(TokenType::DL_RBRACKET, "Expected ']'");
            left = std::make_unique<IndexExpr>(std::move(left), std::move(index), loc);
            continue;
        }

        break;
    }
    return left;
}

} // namespace jspp
