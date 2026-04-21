#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "token.hpp"

namespace jspp {

class Lexer {
public:
    explicit Lexer(const std::string& source, const std::string& filename = "<stdin>");

    std::vector<Token> tokenize();

private:
    std::string source_;
    std::string filename_;
    int pos_    = 0;
    int line_   = 1;
    int column_ = 1;

    char current() const;
    char peek(int offset = 1) const;
    char advance();
    bool isAtEnd() const;
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();

    Token makeToken(TokenType type, const std::string& value, SourceLocation loc);
    SourceLocation currentLoc() const;

    Token readNumber();
    Token readString();
    Token readChar();
    Token readTemplateLiteral();
    Token readIdentifierOrKeyword();
    Token readOperator();

    static const std::unordered_map<std::string, TokenType> keywords_;
    static const std::unordered_map<std::string, TokenType> typeKeywords_;
};

} // namespace jspp
