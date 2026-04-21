#include "lexer.hpp"
#include <stdexcept>

namespace jspp {

// ============================================================================
// Keyword Maps
// ============================================================================

const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"let",          TokenType::KW_LET},
    {"const",        TokenType::KW_CONST},
    {"function",     TokenType::KW_FUNCTION},
    {"return",       TokenType::KW_RETURN},
    {"if",           TokenType::KW_IF},
    {"else",         TokenType::KW_ELSE},
    {"for",          TokenType::KW_FOR},
    {"while",        TokenType::KW_WHILE},
    {"do",           TokenType::KW_DO},
    {"switch",       TokenType::KW_SWITCH},
    {"case",         TokenType::KW_CASE},
    {"default",      TokenType::KW_DEFAULT},
    {"break",        TokenType::KW_BREAK},
    {"continue",     TokenType::KW_CONTINUE},
    {"type",         TokenType::KW_TYPE},
    {"class",        TokenType::KW_CLASS},
    {"new",          TokenType::KW_NEW},
    {"this",         TokenType::KW_THIS},
    {"super",        TokenType::KW_SUPER},
    {"extends",      TokenType::KW_EXTENDS},
    {"import",       TokenType::KW_IMPORT},
    {"from",         TokenType::KW_FROM},
    {"export",       TokenType::KW_EXPORT},
    {"enum",         TokenType::KW_ENUM},
    {"try",          TokenType::KW_TRY},
    {"catch",        TokenType::KW_CATCH},
    {"throw",        TokenType::KW_THROW},
    {"of",           TokenType::KW_OF},
    {"ref",          TokenType::KW_REF},
    {"null",         TokenType::KW_NULL},
    {"true",         TokenType::KW_TRUE},
    {"false",        TokenType::KW_FALSE},
    {"operator",     TokenType::KW_OPERATOR},
    {"cpp",          TokenType::KW_CPP},
    {"cpp_include",  TokenType::KW_CPP_INCLUDE},
    {"as",           TokenType::KW_AS},
    {"shared_new",   TokenType::KW_SHARED_NEW},
};

const std::unordered_map<std::string, TokenType> Lexer::typeKeywords_ = {
    {"int",    TokenType::TYPE_INT},
    {"i8",     TokenType::TYPE_I8},
    {"i16",    TokenType::TYPE_I16},
    {"i32",    TokenType::TYPE_I32},
    {"i64",    TokenType::TYPE_I64},
    {"u8",     TokenType::TYPE_U8},
    {"u16",    TokenType::TYPE_U16},
    {"u32",    TokenType::TYPE_U32},
    {"u64",    TokenType::TYPE_U64},
    {"float",  TokenType::TYPE_FLOAT},
    {"double", TokenType::TYPE_DOUBLE},
    {"bool",   TokenType::TYPE_BOOL},
    {"string", TokenType::TYPE_STRING},
    {"char",   TokenType::TYPE_CHAR},
    {"void",   TokenType::TYPE_VOID},
    {"auto",   TokenType::TYPE_AUTO},
    {"size",   TokenType::TYPE_SIZE},
    {"ptr",    TokenType::TYPE_PTR},
};

// ============================================================================
// Constructor
// ============================================================================

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

// ============================================================================
// Main tokenize loop
// ============================================================================

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;

        char ch = current();

        // String literal
        if (ch == '"') {
            tokens.push_back(readString());
            continue;
        }

        // Char literal: 'x'
        if (ch == '\'' && pos_ + 2 < static_cast<int>(source_.size()) && source_[pos_ + 2] == '\'') {
            tokens.push_back(readChar());
            continue;
        }

        // Template literal
        if (ch == '`') {
            tokens.push_back(readTemplateLiteral());
            continue;
        }

        // Numbers
        if (ch >= '0' && ch <= '9') {
            tokens.push_back(readNumber());
            continue;
        }

        // Identifiers / keywords
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // Operators / delimiters
        tokens.push_back(readOperator());
    }

    tokens.push_back(makeToken(TokenType::TOK_EOF, "", currentLoc()));
    return tokens;
}

// ============================================================================
// Character access
// ============================================================================

char Lexer::current() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peek(int offset) const {
    int idx = pos_ + offset;
    if (idx < 0 || idx >= static_cast<int>(source_.size())) return '\0';
    return source_[idx];
}

char Lexer::advance() {
    char ch = current();
    pos_++;
    column_++;
    return ch;
}

bool Lexer::isAtEnd() const {
    return pos_ >= static_cast<int>(source_.size());
}

// ============================================================================
// Whitespace & comment skipping
// ============================================================================

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char ch = current();
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            advance();
            continue;
        }
        if (ch == '\n') {
            advance();
            line_++;
            column_ = 1;
            continue;
        }
        // Line comment
        if (ch == '/' && peek() == '/') {
            skipLineComment();
            continue;
        }
        // Block comment
        if (ch == '/' && peek() == '*') {
            skipBlockComment();
            continue;
        }
        break;
    }
}

void Lexer::skipLineComment() {
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    advance(); // skip /
    advance(); // skip *
    while (!isAtEnd()) {
        if (current() == '\n') {
            line_++;
            column_ = 0;
        }
        if (current() == '*' && peek() == '/') {
            advance(); // skip *
            advance(); // skip /
            return;
        }
        advance();
    }
}

// ============================================================================
// Token constructors
// ============================================================================

Token Lexer::makeToken(TokenType type, const std::string& value, SourceLocation loc) {
    return Token{type, value, loc};
}

SourceLocation Lexer::currentLoc() const {
    return SourceLocation{filename_, line_, column_};
}

// ============================================================================
// Read methods
// ============================================================================

Token Lexer::readNumber() {
    auto loc = currentLoc();
    std::string num;

    // Hex: 0x...  Binary: 0b...
    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        num += advance(); // '0'
        num += advance(); // 'x'
        while (!isAtEnd() && ((current() >= '0' && current() <= '9') ||
               (current() >= 'a' && current() <= 'f') ||
               (current() >= 'A' && current() <= 'F'))) {
            num += advance();
        }
        return makeToken(TokenType::LIT_HEX, num, loc);
    }
    if (current() == '0' && (peek() == 'b' || peek() == 'B')) {
        num += advance(); // '0'
        num += advance(); // 'b'
        while (!isAtEnd() && (current() == '0' || current() == '1')) {
            num += advance();
        }
        return makeToken(TokenType::LIT_BIN, num, loc);
    }

    // Integer part
    while (!isAtEnd() && current() >= '0' && current() <= '9') {
        num += advance();
    }

    // Floating point
    if (!isAtEnd() && current() == '.' && peek() >= '0' && peek() <= '9') {
        num += advance(); // '.'
        while (!isAtEnd() && current() >= '0' && current() <= '9') {
            num += advance();
        }
        return makeToken(TokenType::LIT_FLOAT, num, loc);
    }

    return makeToken(TokenType::LIT_INT, num, loc);
}

Token Lexer::readString() {
    auto loc = currentLoc();
    advance(); // skip opening "
    std::string val;

    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            advance();
            char esc = current();
            switch (esc) {
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                case '\\': val += '\\'; break;
                case '"':  val += '"';  break;
                case '0':  val += '\0'; break;
                default:   val += esc;  break;
            }
            advance();
        } else {
            val += advance();
        }
    }
    if (!isAtEnd()) advance(); // skip closing "

    return makeToken(TokenType::LIT_STRING, val, loc);
}

Token Lexer::readChar() {
    auto loc = currentLoc();
    advance(); // skip '
    std::string val(1, current());
    advance(); // the char
    advance(); // skip '
    return makeToken(TokenType::LIT_CHAR, val, loc);
}

Token Lexer::readTemplateLiteral() {
    auto loc = currentLoc();
    advance(); // skip `
    std::string raw;

    while (!isAtEnd() && current() != '`') {
        if (current() == '\n') {
            line_++;
            column_ = 0;
        }
        raw += advance();
    }
    if (!isAtEnd()) advance(); // skip `

    return makeToken(TokenType::LIT_TEMPLATE, raw, loc);
}

Token Lexer::readIdentifierOrKeyword() {
    auto loc = currentLoc();
    std::string id;

    while (!isAtEnd() && ((current() >= 'a' && current() <= 'z') ||
           (current() >= 'A' && current() <= 'Z') ||
           (current() >= '0' && current() <= '9') ||
           current() == '_')) {
        id += advance();
    }

    // cpp_include is a compound keyword
    if (id == "cpp" && pos_ + 8 <= static_cast<int>(source_.size()) &&
        source_.substr(pos_, 8) == "_include") {
        for (int i = 0; i < 8; i++) {
            id += advance();
        }
    }

    // Check type keywords first (they override regular keywords)
    auto tit = typeKeywords_.find(id);
    if (tit != typeKeywords_.end()) {
        return makeToken(tit->second, id, loc);
    }

    // Check regular keywords
    auto kit = keywords_.find(id);
    if (kit != keywords_.end()) {
        return makeToken(kit->second, id, loc);
    }

    return makeToken(TokenType::IDENTIFIER, id, loc);
}

Token Lexer::readOperator() {
    auto loc = currentLoc();
    char c0 = current();
    char c1 = peek(1);
    char c2 = peek(2);

    // 3-char operators
    if (c0 == '=' && c1 == '=' && c2 == '=') {
        advance(); advance(); advance();
        return makeToken(TokenType::OP_STRICT_EQ, "===", loc);
    }
    if (c0 == '!' && c1 == '=' && c2 == '=') {
        advance(); advance(); advance();
        return makeToken(TokenType::OP_STRICT_NEQ, "!==", loc);
    }

    // 2-char operators
    if (c0 == '=' && c1 == '>') { advance(); advance(); return makeToken(TokenType::OP_ARROW, "=>", loc); }
    if (c0 == '+' && c1 == '+') { advance(); advance(); return makeToken(TokenType::OP_INCREMENT, "++", loc); }
    if (c0 == '-' && c1 == '-') { advance(); advance(); return makeToken(TokenType::OP_DECREMENT, "--", loc); }
    if (c0 == '+' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_PLUS_ASSIGN, "+=", loc); }
    if (c0 == '-' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_MINUS_ASSIGN, "-=", loc); }
    if (c0 == '*' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_STAR_ASSIGN, "*=", loc); }
    if (c0 == '/' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_SLASH_ASSIGN, "/=", loc); }
    if (c0 == '%' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_PERCENT_ASSIGN, "%=", loc); }
    if (c0 == '<' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_LTE, "<=", loc); }
    if (c0 == '>' && c1 == '=') { advance(); advance(); return makeToken(TokenType::OP_GTE, ">=", loc); }
    if (c0 == '&' && c1 == '&') { advance(); advance(); return makeToken(TokenType::OP_AND, "&&", loc); }
    if (c0 == '|' && c1 == '|') { advance(); advance(); return makeToken(TokenType::OP_OR, "||", loc); }
    if (c0 == '<' && c1 == '<') { advance(); advance(); return makeToken(TokenType::OP_LSHIFT, "<<", loc); }
    if (c0 == '>' && c1 == '>') { advance(); advance(); return makeToken(TokenType::OP_RSHIFT, ">>", loc); }
    if (c0 == ':' && c1 == ':') { advance(); advance(); return makeToken(TokenType::OP_SCOPE, "::", loc); }

    // 1-char operators and delimiters
    advance();
    switch (c0) {
        case '+': return makeToken(TokenType::OP_PLUS, "+", loc);
        case '-': return makeToken(TokenType::OP_MINUS, "-", loc);
        case '*': return makeToken(TokenType::OP_STAR, "*", loc);
        case '/': return makeToken(TokenType::OP_SLASH, "/", loc);
        case '%': return makeToken(TokenType::OP_PERCENT, "%", loc);
        case '=': return makeToken(TokenType::OP_ASSIGN, "=", loc);
        case '<': return makeToken(TokenType::OP_LT, "<", loc);
        case '>': return makeToken(TokenType::OP_GT, ">", loc);
        case '!': return makeToken(TokenType::OP_NOT, "!", loc);
        case '&': return makeToken(TokenType::OP_BIT_AND, "&", loc);
        case '|': return makeToken(TokenType::OP_BIT_OR, "|", loc);
        case '^': return makeToken(TokenType::OP_BIT_XOR, "^", loc);
        case '~': return makeToken(TokenType::OP_BIT_NOT, "~", loc);
        case '.': return makeToken(TokenType::OP_DOT, ".", loc);
        case '?': return makeToken(TokenType::OP_QUESTION, "?", loc);
        case ':': return makeToken(TokenType::OP_COLON, ":", loc);
        case '(': return makeToken(TokenType::DL_LPAREN, "(", loc);
        case ')': return makeToken(TokenType::DL_RPAREN, ")", loc);
        case '{': return makeToken(TokenType::DL_LBRACE, "{", loc);
        case '}': return makeToken(TokenType::DL_RBRACE, "}", loc);
        case '[': return makeToken(TokenType::DL_LBRACKET, "[", loc);
        case ']': return makeToken(TokenType::DL_RBRACKET, "]", loc);
        case ';': return makeToken(TokenType::DL_SEMICOLON, ";", loc);
        case ',': return makeToken(TokenType::DL_COMMA, ",", loc);
        default:
            return makeToken(TokenType::UNKNOWN, std::string(1, c0), loc);
    }
}

} // namespace jspp
