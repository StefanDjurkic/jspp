# JSPP Compiler Architecture

## Overview

The JSPP compiler transforms `.jspp` source files into readable, modern C++20 code. It then invokes a system C++ compiler (g++/clang++) to produce native binaries.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          JSPP Compiler Pipeline                         │
│                                                                         │
│   .jspp ──► Lexer ──► Parser ──► Type Checker ──► CodeGen ──► .cpp     │
│                                                         │               │
│                                                         ▼               │
│                                                    g++ / clang++        │
│                                                         │               │
│                                                         ▼               │
│                                                    native binary        │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
jspp/
├── docs/
│   ├── LANGUAGE_SPEC.md          # Full language specification
│   ├── TOKENS.md                 # Token definitions
│   ├── GRAMMAR.md                # EBNF grammar
│   └── ARCHITECTURE.md           # This file
│
├── src/
│   ├── main.cpp                  # CLI entry point / driver
│   │
│   ├── lexer/
│   │   ├── lexer.hpp             # Lexer class declaration
│   │   ├── lexer.cpp             # Lexer implementation
│   │   ├── token.hpp             # Token struct + TokenType enum
│   │   └── token.cpp             # Token utility functions
│   │
│   ├── parser/
│   │   ├── parser.hpp            # Parser class declaration
│   │   ├── parser.cpp            # Recursive descent parser
│   │   └── ast.hpp               # All AST node definitions
│   │
│   ├── typechecker/
│   │   ├── typechecker.hpp       # Type checker declaration
│   │   ├── typechecker.cpp       # Type inference + validation
│   │   ├── types.hpp             # Type system definitions
│   │   └── scope.hpp             # Symbol table / scope management
│   │
│   ├── codegen/
│   │   ├── codegen.hpp           # Code generator declaration
│   │   ├── codegen.cpp           # AST → C++ emission
│   │   └── cpp_emitter.hpp       # C++ output formatting helpers
│   │
│   └── common/
│       ├── error.hpp             # Error reporting utilities
│       ├── error.cpp             # Error formatting + source locations
│       └── source_location.hpp   # File/line/column tracking
│
├── examples/
│   ├── hello.jspp                # Hello world
│   ├── fibonacci.jspp            # Recursive + iterative fib
│   ├── structs.jspp              # Struct usage
│   ├── classes.jspp              # Class + inheritance
│   ├── generics.jspp             # Template functions
│   ├── cpp_interop.jspp          # Raw C++ blocks
│   └── game_entity.jspp          # Practical game example
│
├── tests/
│   ├── lexer_tests.cpp
│   ├── parser_tests.cpp
│   ├── typechecker_tests.cpp
│   └── codegen_tests.cpp
│
├── CMakeLists.txt                # Build system
└── README.md                     # Project README
```

---

## Stage 1: Lexer

**Input:** Raw `.jspp` source text  
**Output:** `std::vector<Token>`

### Responsibilities
- Scan characters left to right
- Produce tokens with type, value, line, and column
- Handle string escapes, template literals, hex/binary literals
- Skip whitespace and comments
- Report errors for unrecognized characters

### Key Design Decisions
- Longest-match tokenization (e.g., `===` before `==` before `=`)
- Keywords recognized after identifier scan (lookup table)
- Template literals require recursive sub-lexing for `${expr}`
- Line/column tracking for all tokens (for error messages)

### Token Flow
```
"let x: int = 42;"

→ [KW_LET, IDENT("x"), OP_COLON, TYPE_INT, OP_ASSIGN, LIT_INT("42"), DL_SEMICOLON, EOF]
```

---

## Stage 2: Parser

**Input:** `std::vector<Token>`  
**Output:** `AST` (Abstract Syntax Tree)

### Approach: Recursive Descent
- Top-down parsing, one function per grammar rule
- Lookahead of 1–2 tokens (LL(2) in some cases)
- Pratt parsing for expressions (operator precedence)

### Key Functions
| Function            | Parses                                   |
|---------------------|------------------------------------------|
| `parseProgram()`    | Top-level statements until EOF           |
| `parseFunction()`   | `function name(params): type { body }`   |
| `parseVariable()`   | `let/const name [: type] = expr;`        |
| `parseType()`       | Type declarations `type Name = { ... };` |
| `parseClass()`      | Class declarations with inheritance      |
| `parseEnum()`       | Enum declarations                        |
| `parseBlock()`      | `{ statement* }`                         |
| `parseIf()`         | If / else if / else chains               |
| `parseFor()`        | C-style for and for-of                   |
| `parseWhile()`      | While and do-while                       |
| `parseSwitch()`     | Switch/case/default                      |
| `parseTryCatch()`   | Try/catch blocks                         |
| `parseExpression()` | Pratt parser with precedence climbing    |
| `parseImport()`     | Import declarations                      |
| `parseCppBlock()`   | Raw C++ pass-through                     |

### Expression Precedence (lowest to highest)
1. Assignment (`=`, `+=`, `-=`, `*=`, `/=`, `%=`)
2. Ternary (`? :`)
3. Logical OR (`||`)
4. Logical AND (`&&`)
5. Bitwise OR (`|`)
6. Bitwise XOR (`^`)
7. Bitwise AND (`&`)
8. Equality (`===`, `!==`)
9. Relational (`<`, `>`, `<=`, `>=`)
10. Shift (`<<`, `>>`)
11. Additive (`+`, `-`)
12. Multiplicative (`*`, `/`, `%`)
13. Unary (`!`, `-`, `~`, `++`, `--`)
14. Postfix (`++`, `--`, `()`, `[]`, `.`)
15. Primary (literals, identifiers, parenthesized)

---

## Stage 3: Type Checker

**Input:** Untyped/partially-typed AST  
**Output:** Fully-typed AST with all types resolved

### Responsibilities
1. **Type inference** — Deduce types from literals and expressions
2. **Type validation** — Ensure operations are type-safe
3. **Symbol resolution** — Build and query scope/symbol tables
4. **Error detection** — Flag type mismatches, undeclared variables, etc.

### Symbol Table
```
Scope {
    parent: Scope?
    symbols: Map<string, Symbol>
}

Symbol {
    name: string
    type: Type
    mutable: bool
    kind: Variable | Function | Type | Class | Enum
}
```

### Type Inference Rules
| Expression               | Inferred Type          |
|--------------------------|------------------------|
| `42`                     | `int`                  |
| `3.14`                   | `double`               |
| `"hello"`                | `string`               |
| `'c'`                    | `char`                 |
| `true` / `false`         | `bool`                 |
| `[1, 2, 3]`             | `int[]`                |
| `{ x: 1, y: 2 }`       | anonymous struct       |
| `a + b` (int, int)      | `int`                  |
| `a + b` (string, string)| `string`               |
| `a > b`                  | `bool`                 |
| `fn(args)`               | return type of `fn`    |

### Errors Detected
- Undeclared variable / function
- Type mismatch in assignment
- Type mismatch in binary operation
- Wrong number of function arguments
- Wrong argument types
- Assigning to `const` variable
- Using `==` instead of `===`
- Return type mismatch
- Missing return statement

---

## Stage 4: Code Generator

**Input:** Fully-typed AST  
**Output:** `.cpp` and `.hpp` files

### Strategy
- Walk the typed AST depth-first
- Emit clean, readable, idiomatic C++20
- Preserve original variable/function names
- Generate anonymous struct names for object literals
- Emit `#include` directives based on used features

### Auto-Included Headers
The code generator tracks which features are used and includes only necessary headers:

| Feature Used       | Header Added            |
|--------------------|-------------------------|
| `string`           | `<string>`              |
| Arrays             | `<vector>`              |
| `console.log`      | `<iostream>`            |
| `Math.*`           | `<cmath>`               |
| Template strings   | `<format>`              |
| `unique_ptr`       | `<memory>`              |
| `shared_ptr`       | `<memory>`              |
| `optional`         | `<optional>`            |
| `.includes()`      | `<algorithm>`           |
| `.indexOf()`       | `<algorithm>`           |
| Exceptions         | `<stdexcept>`           |

### Translation Patterns

#### Variable declarations
```
let x: int = 5;          →  int x = 5;
const y = "hi";          →  const std::string y = "hi";
```

#### Functions
```
function f(a: int): int   →  int f(int a) { ... }
```

#### Arrow functions
```
(x: int): int => x * 2   →  [](int x) -> int { return x * 2; }
```

#### Object literals → anonymous structs
```
{ x: 1, y: 2 }           →  struct __anon_N { int x; int y; }; __anon_N var = {1, 2};
```

#### Console
```
console.log("hi", x)     →  std::cout << "hi" << " " << x << std::endl;
```

#### For-of
```
for (let x of arr)        →  for (auto& x : arr)
```

#### Strict equality
```
a === b                   →  a == b
a !== b                   →  a != b
```

#### Null / optional
```
let x: int? = null        →  std::optional<int> x = std::nullopt;
```

---

## Stage 5: Driver (CLI)

**Usage:**
```bash
jspp input.jspp                      # Compile to input.cpp
jspp input.jspp -o output.cpp        # Compile to specified output
jspp input.jspp -o output.cpp --run  # Compile + invoke g++ + run
jspp input.jspp --emit-ast           # Print AST (debug)
jspp input.jspp --emit-tokens        # Print token stream (debug)
```

### Driver Steps
1. Parse CLI arguments
2. Read `.jspp` source file
3. Run Lexer → tokens
4. Run Parser → AST
5. Run Type Checker → typed AST
6. Run Code Generator → `.cpp` output
7. (Optional) Invoke system compiler: `g++ -std=c++20 -O3 output.cpp -o program`
8. (Optional) Run the resulting binary

### Error Reporting Format
```
error[E001]: type mismatch
  --> main.jspp:12:5
   |
12 |     let x: int = "hello";
   |                  ^^^^^^^ expected 'int', found 'string'
```

---

## Implementation Language

The compiler itself is written in **C++20** for:
- Native performance during compilation
- Direct understanding of the target language
- No runtime dependencies
- Easy integration with the system C++ toolchain

### Build System
CMake is used for building the compiler:
```bash
mkdir build && cd build
cmake ..
make
```

---

## Extension Points (Future)

- **Optimizer pass** — between type checker and codegen
- **Module system** — multi-file compilation with dependency resolution
- **LSP server** — IDE integration for autocomplete/diagnostics
- **REPL** — interactive mode
- **Package manager** — dependency management
- **Debug info** — source maps from .jspp to generated .cpp
