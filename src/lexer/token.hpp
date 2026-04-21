#pragma once

#include <string>
#include <ostream>

namespace jspp {

enum class TokenType {
    // Keywords
    KW_LET, KW_CONST, KW_FUNCTION, KW_RETURN,
    KW_IF, KW_ELSE, KW_FOR, KW_WHILE, KW_DO,
    KW_SWITCH, KW_CASE, KW_DEFAULT, KW_BREAK, KW_CONTINUE,
    KW_TYPE, KW_CLASS, KW_NEW, KW_SHARED_NEW,
    KW_CONSTRUCTOR, KW_THIS, KW_SUPER, KW_EXTENDS,
    KW_IMPORT, KW_FROM, KW_EXPORT, KW_ENUM,
    KW_TRY, KW_CATCH, KW_THROW,
    KW_OF, KW_REF, KW_NULL, KW_TRUE, KW_FALSE,
    KW_OPERATOR, KW_CPP, KW_CPP_INCLUDE, KW_AS,

    // Type keywords
    TYPE_INT, TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_FLOAT, TYPE_DOUBLE, TYPE_BOOL, TYPE_STRING,
    TYPE_CHAR, TYPE_VOID, TYPE_AUTO, TYPE_SIZE, TYPE_PTR,

    // Literals
    LIT_INT, LIT_HEX, LIT_BIN, LIT_FLOAT,
    LIT_STRING, LIT_TEMPLATE, LIT_CHAR,

    // Operators
    OP_PLUS, OP_MINUS, OP_STAR, OP_SLASH, OP_PERCENT,
    OP_ASSIGN,
    OP_PLUS_ASSIGN, OP_MINUS_ASSIGN, OP_STAR_ASSIGN,
    OP_SLASH_ASSIGN, OP_PERCENT_ASSIGN,
    OP_STRICT_EQ, OP_STRICT_NEQ,
    OP_LT, OP_GT, OP_LTE, OP_GTE,
    OP_AND, OP_OR, OP_NOT,
    OP_BIT_AND, OP_BIT_OR, OP_BIT_XOR, OP_BIT_NOT,
    OP_LSHIFT, OP_RSHIFT,
    OP_INCREMENT, OP_DECREMENT,
    OP_ARROW,     // =>
    OP_DOT,       // .
    OP_QUESTION,  // ?
    OP_COLON,     // :
    OP_SCOPE,     // ::

    // Delimiters
    DL_LPAREN, DL_RPAREN,
    DL_LBRACE, DL_RBRACE,
    DL_LBRACKET, DL_RBRACKET,
    DL_SEMICOLON, DL_COMMA,

    // Special
    IDENTIFIER,
    NEWLINE,
    TOK_EOF,
    UNKNOWN
};

struct SourceLocation {
    std::string filename;
    int line   = 1;
    int column = 1;
};

struct Token {
    TokenType    type   = TokenType::UNKNOWN;
    std::string  value;
    SourceLocation loc;
};

// Convert TokenType to human-readable string
inline const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::KW_LET:           return "KW_LET";
        case TokenType::KW_CONST:         return "KW_CONST";
        case TokenType::KW_FUNCTION:      return "KW_FUNCTION";
        case TokenType::KW_RETURN:        return "KW_RETURN";
        case TokenType::KW_IF:            return "KW_IF";
        case TokenType::KW_ELSE:          return "KW_ELSE";
        case TokenType::KW_FOR:           return "KW_FOR";
        case TokenType::KW_WHILE:         return "KW_WHILE";
        case TokenType::KW_DO:            return "KW_DO";
        case TokenType::KW_SWITCH:        return "KW_SWITCH";
        case TokenType::KW_CASE:          return "KW_CASE";
        case TokenType::KW_DEFAULT:       return "KW_DEFAULT";
        case TokenType::KW_BREAK:         return "KW_BREAK";
        case TokenType::KW_CONTINUE:      return "KW_CONTINUE";
        case TokenType::KW_TYPE:          return "KW_TYPE";
        case TokenType::KW_CLASS:         return "KW_CLASS";
        case TokenType::KW_NEW:           return "KW_NEW";
        case TokenType::KW_SHARED_NEW:    return "KW_SHARED_NEW";
        case TokenType::KW_CONSTRUCTOR:   return "KW_CONSTRUCTOR";
        case TokenType::KW_THIS:          return "KW_THIS";
        case TokenType::KW_SUPER:         return "KW_SUPER";
        case TokenType::KW_EXTENDS:       return "KW_EXTENDS";
        case TokenType::KW_IMPORT:        return "KW_IMPORT";
        case TokenType::KW_FROM:          return "KW_FROM";
        case TokenType::KW_EXPORT:        return "KW_EXPORT";
        case TokenType::KW_ENUM:          return "KW_ENUM";
        case TokenType::KW_TRY:           return "KW_TRY";
        case TokenType::KW_CATCH:         return "KW_CATCH";
        case TokenType::KW_THROW:         return "KW_THROW";
        case TokenType::KW_OF:            return "KW_OF";
        case TokenType::KW_REF:           return "KW_REF";
        case TokenType::KW_NULL:          return "KW_NULL";
        case TokenType::KW_TRUE:          return "KW_TRUE";
        case TokenType::KW_FALSE:         return "KW_FALSE";
        case TokenType::KW_OPERATOR:      return "KW_OPERATOR";
        case TokenType::KW_CPP:           return "KW_CPP";
        case TokenType::KW_CPP_INCLUDE:   return "KW_CPP_INCLUDE";
        case TokenType::KW_AS:            return "KW_AS";

        case TokenType::TYPE_INT:         return "TYPE_INT";
        case TokenType::TYPE_I8:          return "TYPE_I8";
        case TokenType::TYPE_I16:         return "TYPE_I16";
        case TokenType::TYPE_I32:         return "TYPE_I32";
        case TokenType::TYPE_I64:         return "TYPE_I64";
        case TokenType::TYPE_U8:          return "TYPE_U8";
        case TokenType::TYPE_U16:         return "TYPE_U16";
        case TokenType::TYPE_U32:         return "TYPE_U32";
        case TokenType::TYPE_U64:         return "TYPE_U64";
        case TokenType::TYPE_FLOAT:       return "TYPE_FLOAT";
        case TokenType::TYPE_DOUBLE:      return "TYPE_DOUBLE";
        case TokenType::TYPE_BOOL:        return "TYPE_BOOL";
        case TokenType::TYPE_STRING:      return "TYPE_STRING";
        case TokenType::TYPE_CHAR:        return "TYPE_CHAR";
        case TokenType::TYPE_VOID:        return "TYPE_VOID";
        case TokenType::TYPE_AUTO:        return "TYPE_AUTO";
        case TokenType::TYPE_SIZE:        return "TYPE_SIZE";
        case TokenType::TYPE_PTR:         return "TYPE_PTR";

        case TokenType::LIT_INT:          return "LIT_INT";
        case TokenType::LIT_HEX:          return "LIT_HEX";
        case TokenType::LIT_BIN:          return "LIT_BIN";
        case TokenType::LIT_FLOAT:        return "LIT_FLOAT";
        case TokenType::LIT_STRING:       return "LIT_STRING";
        case TokenType::LIT_TEMPLATE:     return "LIT_TEMPLATE";
        case TokenType::LIT_CHAR:         return "LIT_CHAR";

        case TokenType::OP_PLUS:          return "OP_PLUS";
        case TokenType::OP_MINUS:         return "OP_MINUS";
        case TokenType::OP_STAR:          return "OP_STAR";
        case TokenType::OP_SLASH:         return "OP_SLASH";
        case TokenType::OP_PERCENT:       return "OP_PERCENT";
        case TokenType::OP_ASSIGN:        return "OP_ASSIGN";
        case TokenType::OP_PLUS_ASSIGN:   return "OP_PLUS_ASSIGN";
        case TokenType::OP_MINUS_ASSIGN:  return "OP_MINUS_ASSIGN";
        case TokenType::OP_STAR_ASSIGN:   return "OP_STAR_ASSIGN";
        case TokenType::OP_SLASH_ASSIGN:  return "OP_SLASH_ASSIGN";
        case TokenType::OP_PERCENT_ASSIGN:return "OP_PERCENT_ASSIGN";
        case TokenType::OP_STRICT_EQ:     return "OP_STRICT_EQ";
        case TokenType::OP_STRICT_NEQ:    return "OP_STRICT_NEQ";
        case TokenType::OP_LT:            return "OP_LT";
        case TokenType::OP_GT:            return "OP_GT";
        case TokenType::OP_LTE:           return "OP_LTE";
        case TokenType::OP_GTE:           return "OP_GTE";
        case TokenType::OP_AND:           return "OP_AND";
        case TokenType::OP_OR:            return "OP_OR";
        case TokenType::OP_NOT:           return "OP_NOT";
        case TokenType::OP_BIT_AND:       return "OP_BIT_AND";
        case TokenType::OP_BIT_OR:        return "OP_BIT_OR";
        case TokenType::OP_BIT_XOR:       return "OP_BIT_XOR";
        case TokenType::OP_BIT_NOT:       return "OP_BIT_NOT";
        case TokenType::OP_LSHIFT:        return "OP_LSHIFT";
        case TokenType::OP_RSHIFT:        return "OP_RSHIFT";
        case TokenType::OP_INCREMENT:     return "OP_INCREMENT";
        case TokenType::OP_DECREMENT:     return "OP_DECREMENT";
        case TokenType::OP_ARROW:         return "OP_ARROW";
        case TokenType::OP_DOT:           return "OP_DOT";
        case TokenType::OP_QUESTION:      return "OP_QUESTION";
        case TokenType::OP_COLON:         return "OP_COLON";
        case TokenType::OP_SCOPE:         return "OP_SCOPE";

        case TokenType::DL_LPAREN:        return "DL_LPAREN";
        case TokenType::DL_RPAREN:        return "DL_RPAREN";
        case TokenType::DL_LBRACE:        return "DL_LBRACE";
        case TokenType::DL_RBRACE:        return "DL_RBRACE";
        case TokenType::DL_LBRACKET:      return "DL_LBRACKET";
        case TokenType::DL_RBRACKET:      return "DL_RBRACKET";
        case TokenType::DL_SEMICOLON:     return "DL_SEMICOLON";
        case TokenType::DL_COMMA:         return "DL_COMMA";

        case TokenType::IDENTIFIER:       return "IDENTIFIER";
        case TokenType::NEWLINE:          return "NEWLINE";
        case TokenType::TOK_EOF:          return "TOK_EOF";
        case TokenType::UNKNOWN:          return "UNKNOWN";
    }
    return "UNKNOWN";
}

inline std::ostream& operator<<(std::ostream& os, const Token& tok) {
    os << tokenTypeName(tok.type) << "(\"" << tok.value << "\") at "
       << tok.loc.line << ":" << tok.loc.column;
    return os;
}

} // namespace jspp
