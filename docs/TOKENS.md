# JSPP Token Definitions

## Token Categories

### 1. Keywords

| Token Name         | Lexeme        | Description                      |
|--------------------|---------------|----------------------------------|
| `KW_LET`           | `let`         | Mutable variable declaration     |
| `KW_CONST`         | `const`       | Immutable variable declaration   |
| `KW_FUNCTION`      | `function`    | Function declaration             |
| `KW_RETURN`        | `return`      | Return statement                 |
| `KW_IF`            | `if`          | Conditional branch               |
| `KW_ELSE`          | `else`        | Else branch                      |
| `KW_FOR`           | `for`         | For loop                         |
| `KW_WHILE`         | `while`       | While loop                       |
| `KW_DO`            | `do`          | Do-while loop                    |
| `KW_SWITCH`        | `switch`      | Switch statement                 |
| `KW_CASE`          | `case`        | Switch case                      |
| `KW_DEFAULT`       | `default`     | Switch default                   |
| `KW_BREAK`         | `break`       | Break statement                  |
| `KW_CONTINUE`      | `continue`    | Continue statement               |
| `KW_TYPE`          | `type`        | Struct/type alias declaration    |
| `KW_CLASS`         | `class`       | Class declaration                |
| `KW_NEW`           | `new`         | Heap allocation (unique_ptr)     |
| `KW_SHARED_NEW`    | `shared_new`  | Heap allocation (shared_ptr)     |
| `KW_CONSTRUCTOR`   | `constructor` | Class constructor                |
| `KW_THIS`          | `this`        | Self reference                   |
| `KW_SUPER`         | `super`       | Parent class reference           |
| `KW_EXTENDS`       | `extends`     | Class inheritance                |
| `KW_IMPORT`        | `import`      | Module import                    |
| `KW_FROM`          | `from`        | Import source                    |
| `KW_EXPORT`        | `export`      | Module export                    |
| `KW_ENUM`          | `enum`        | Enum declaration                 |
| `KW_TRY`           | `try`         | Try block                        |
| `KW_CATCH`         | `catch`       | Catch block                      |
| `KW_THROW`         | `throw`       | Throw exception                  |
| `KW_OF`            | `of`          | For-of loop                      |
| `KW_REF`           | `ref`         | Pass by reference                |
| `KW_NULL`          | `null`        | Null literal                     |
| `KW_TRUE`          | `true`        | Boolean true                     |
| `KW_FALSE`         | `false`       | Boolean false                    |
| `KW_OPERATOR`      | `operator`    | Operator overload                |
| `KW_CPP`           | `cpp`         | Raw C++ block                    |
| `KW_CPP_INCLUDE`   | `cpp_include` | C++ include directive            |
| `KW_AS`            | `as`          | Import alias                     |

### 2. Type Keywords

| Token Name         | Lexeme     | C++ Mapping        |
|--------------------|------------|---------------------|
| `TYPE_INT`         | `int`      | `int`              |
| `TYPE_I8`          | `i8`       | `int8_t`           |
| `TYPE_I16`         | `i16`      | `int16_t`          |
| `TYPE_I32`         | `i32`      | `int32_t`          |
| `TYPE_I64`         | `i64`      | `int64_t`          |
| `TYPE_U8`          | `u8`       | `uint8_t`          |
| `TYPE_U16`         | `u16`      | `uint16_t`         |
| `TYPE_U32`         | `u32`      | `uint32_t`         |
| `TYPE_U64`         | `u64`      | `uint64_t`         |
| `TYPE_FLOAT`       | `float`    | `float`            |
| `TYPE_DOUBLE`      | `double`   | `double`           |
| `TYPE_BOOL`        | `bool`     | `bool`             |
| `TYPE_STRING`      | `string`   | `std::string`      |
| `TYPE_CHAR`        | `char`     | `char`             |
| `TYPE_VOID`        | `void`     | `void`             |
| `TYPE_AUTO`        | `auto`     | `auto`             |
| `TYPE_SIZE`        | `size`     | `size_t`           |
| `TYPE_PTR`         | `ptr`      | raw pointer        |

### 3. Literals

| Token Name         | Pattern                      | Examples           |
|--------------------|------------------------------|--------------------|
| `LIT_INT`          | `[0-9]+`                     | `42`, `0`, `1000`  |
| `LIT_HEX`         | `0x[0-9a-fA-F]+`            | `0xFF`, `0x1A`     |
| `LIT_BIN`         | `0b[01]+`                    | `0b1010`           |
| `LIT_FLOAT`       | `[0-9]+\.[0-9]+([eE][+-]?[0-9]+)?` | `3.14`, `1.0e10` |
| `LIT_STRING`      | `"[^"]*"` (with escapes)    | `"hello"`          |
| `LIT_TEMPLATE`    | `` `...${expr}...` ``       | `` `hi ${name}` `` |
| `LIT_CHAR`        | `'[^']'`                     | `'a'`, `'\n'`      |

### 4. Operators

| Token Name         | Lexeme  | Description          |
|--------------------|---------|----------------------|
| `OP_PLUS`          | `+`     | Addition             |
| `OP_MINUS`         | `-`     | Subtraction          |
| `OP_STAR`          | `*`     | Multiplication       |
| `OP_SLASH`         | `/`     | Division             |
| `OP_PERCENT`       | `%`     | Modulo               |
| `OP_ASSIGN`        | `=`     | Assignment           |
| `OP_PLUS_ASSIGN`   | `+=`    | Add-assign           |
| `OP_MINUS_ASSIGN`  | `-=`    | Sub-assign           |
| `OP_STAR_ASSIGN`   | `*=`    | Mul-assign           |
| `OP_SLASH_ASSIGN`  | `/=`    | Div-assign           |
| `OP_PERCENT_ASSIGN`| `%=`    | Mod-assign           |
| `OP_STRICT_EQ`     | `===`   | Strict equality      |
| `OP_STRICT_NEQ`    | `!==`   | Strict inequality    |
| `OP_LT`            | `<`     | Less than            |
| `OP_GT`            | `>`     | Greater than         |
| `OP_LTE`           | `<=`    | Less or equal        |
| `OP_GTE`           | `>=`    | Greater or equal     |
| `OP_AND`           | `&&`    | Logical AND          |
| `OP_OR`            | `\|\|`  | Logical OR           |
| `OP_NOT`           | `!`     | Logical NOT          |
| `OP_BIT_AND`       | `&`     | Bitwise AND          |
| `OP_BIT_OR`        | `\|`    | Bitwise OR           |
| `OP_BIT_XOR`       | `^`     | Bitwise XOR          |
| `OP_BIT_NOT`       | `~`     | Bitwise NOT          |
| `OP_LSHIFT`        | `<<`    | Left shift           |
| `OP_RSHIFT`        | `>>`    | Right shift          |
| `OP_INCREMENT`     | `++`    | Increment            |
| `OP_DECREMENT`     | `--`    | Decrement            |
| `OP_ARROW`         | `=>`    | Arrow function       |
| `OP_DOT`           | `.`     | Member access        |
| `OP_QUESTION`      | `?`     | Optional / ternary   |
| `OP_COLON`         | `:`     | Type annotation      |
| `OP_SCOPE`         | `::`    | Namespace scope      |

### 5. Delimiters

| Token Name         | Lexeme | Description       |
|--------------------|--------|-------------------|
| `DL_LPAREN`        | `(`    | Left parenthesis  |
| `DL_RPAREN`        | `)`    | Right parenthesis |
| `DL_LBRACE`        | `{`    | Left brace        |
| `DL_RBRACE`        | `}`    | Right brace       |
| `DL_LBRACKET`      | `[`    | Left bracket      |
| `DL_RBRACKET`      | `]`    | Right bracket     |
| `DL_SEMICOLON`     | `;`    | Statement end     |
| `DL_COMMA`         | `,`    | Separator         |

### 6. Special

| Token Name         | Description                 |
|--------------------|-----------------------------|
| `IDENTIFIER`       | User-defined names          |
| `NEWLINE`          | Line break (for error msgs) |
| `WHITESPACE`       | Spaces/tabs (skipped)       |
| `COMMENT_LINE`     | `// ...` (skipped)          |
| `COMMENT_BLOCK`    | `/* ... */` (skipped)       |
| `EOF`              | End of file                 |
| `UNKNOWN`          | Unrecognized character      |

---

## Token Structure

```
Token {
    type:    TokenType     // enum value from above
    value:   string        // raw lexeme text
    line:    int           // source line number (1-based)
    column:  int           // source column number (1-based)
}
```

## Lexer Precedence Rules

1. **Longest match wins** — `===` before `==` before `=`
2. **Keywords before identifiers** — `let` is KW_LET, not IDENTIFIER
3. **Type keywords before identifiers** — `int` is TYPE_INT, not IDENTIFIER  
4. **Comments are consumed and discarded** — not emitted as tokens
5. **Whitespace is consumed and discarded** — except inside strings
6. **Template strings** — backtick-delimited, `${}` triggers sub-lexing
7. **String escape sequences** — `\n`, `\t`, `\\`, `\"`, `\0` supported
