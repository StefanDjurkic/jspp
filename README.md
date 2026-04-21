# JSPP — JavaScript syntax, C++ performance

JSPP is a statically-typed language that reads like modern JavaScript and compiles to high-performance C++. It ships with:

- A **C++ compiler** (lexer → parser → type checker → code generator → `.cpp` output).
- A **JavaScript reference interpreter** (`prototype/jspp.mjs`) that runs `.jspp` files directly under Node — used for fast iteration, the stdlib, and the test suite.
- A native **REPL window** (`jspp-ship`) built on top of [sokol](https://github.com/floooh/sokol).
- A **regression test suite** covering variables, functions, control flow, arrays, objects, closures, classes, switch/enums, arrow functions, `for…of`, try/catch, imports, and a broad JavaScript-style standard library (Math, JSON, Array/String methods, Date, RegExp, lodash-style helpers, EventEmitter, uuid, path, date-fns, query-string, ms, invariant/warning/assert, etc.).

## Example

```jspp
function main(): int {
    let name = "World";
    console.log(`Hello, ${name}!`);

    let nums: int[] = [1, 2, 3, 4, 5];
    for (let n of nums) {
        console.log(n * n);
    }
    return 0;
}
```

**Compiles to:**

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <format>

int main() {
    std::string name = "World";
    std::cout << std::format("Hello, {}!", name) << std::endl;

    std::vector<int> nums = {1, 2, 3, 4, 5};
    for (auto& n : nums) {
        std::cout << n * n << std::endl;
    }
    return 0;
}
```

## Project layout

```
src/         C++ compiler sources (lexer, parser, typechecker, codegen)
prototype/   Node.js reference interpreter + type checker
examples/    Sample .jspp programs
tests/       Regression test suite (see tests/run.mjs)
docs/        Language spec, grammar, token list, architecture notes
ship/        Native REPL window host (sokol)
third_party/ Vendored sokol headers
```

## Requirements

- **C++ compiler:** A C++20 compiler (MSVC, clang ≥ 14, gcc ≥ 12) and CMake ≥ 3.20.
- **Reference interpreter / tests:** Node.js ≥ 20.

## Build the C++ compiler

```bash
cmake -S . -B build
cmake --build build
```

The resulting binary is `build/jspp` (or `build/Debug/jspp.exe` on Windows).

## Use the compiler

```bash
jspp input.jspp -o output.cpp          # Compile to C++
jspp input.jspp -o output.cpp --run    # Compile, build, and run
jspp input.jspp --emit-tokens          # Debug: print tokens
jspp input.jspp --emit-ast             # Debug: print AST
```

## Run `.jspp` files directly (no C++ build required)

The prototype interpreter runs any `.jspp` file on Node:

```bash
node prototype/jspp.mjs examples/hello.jspp --run
```

## Run the regression tests

```bash
node tests/run.mjs
```

Each test pair is `tests/NN_name.jspp` + `tests/NN_name.expected`. The runner executes the `.jspp` file with the interpreter and diffs the output against the expected file.

## Native REPL (jspp-ship)

```bash
cmake --build build --target jspp-ship
```

`jspp-ship` is an additive, minimal native REPL window with a render area on top and a text area on the bottom. Hotkeys:

- `Enter` — newline
- `Ctrl+Enter` / `F5` — execute
- `Left` / `Right` / `Home` / `End` / `Backspace` / `Delete` / `Tab` — standard editing
- `Up` / `Down` — within-buffer navigation, falling back to history at edges
- `Shift` + arrows / `Ctrl+A` / mouse drag — selection
- `Ctrl+Z` / `Ctrl+Y` — undo / redo
- `Ctrl+C` / `Ctrl+X` / `Ctrl+V` — copy / cut / paste

Built-in draw helpers: `drawRect(...)`, `drawCircle(...)`, `drawLine(...)`, `clear()`.

Self-test target:

```bash
cmake --build build --target jspp-ship-selftest
```

## Key features

- JavaScript-style syntax: `let`, `const`, `function`, arrow functions, classes, `for…of`, destructuring, template literals.
- Static types with inference; optional explicit annotations.
- Zero-GC, stack-preferred codegen.
- `cpp { ... }` escape hatch to drop raw C++ anywhere.
- Large JS-style stdlib available in the reference interpreter (see the `tests/` folder for examples).

## Optional: ZeroEngine browser target

JSPP can also run inside a WebGPU browser host called **[ZeroEngine](https://github.com/StefanDjurkic/zeroengine)**, which exposes drawing builtins (`drawRect`, `drawCircle`, `drawLine`, `clear`) to `.jspp` programs and provides a Playwright-based browser regression harness. ZeroEngine is a **separate, optional project** and is not required to build, run, or test JSPP.

If you want to try it, clone it as a **sibling** of this repo so its test harness can find `tests/run.mjs`:

```
your-workspace/
├── jspp/          # this repo
└── zeroengine/    # optional, separate repo — https://github.com/StefanDjurkic/zeroengine
```

See the [ZeroEngine repository](https://github.com/StefanDjurkic/zeroengine) for setup instructions.

## Documentation

- [Language spec](docs/LANGUAGE_SPEC.md)
- [Tokens](docs/TOKENS.md)
- [Grammar (EBNF)](docs/GRAMMAR.md)
- [Architecture](docs/ARCHITECTURE.md)

## Contributing

JSPP is a fun side project and contributions are very welcome — issues, PRs, bug reports, test cases, stdlib additions, codegen improvements, or just ideas. No formal process; open an issue or PR and we'll chat.

## License

MIT — see [LICENSE](LICENSE). Third-party code under `third_party/` keeps its original license.
# JSPP — JavaScript Syntax, C++ Performance

JSPP is a compiled language that uses JavaScript-like syntax but produces high-performance C++ code. Write in JS, run at C++ speed.

## Quick Example

```jspp
function main(): int {
    let name = "World";
    console.log(`Hello, ${name}!`);

    let nums: int[] = [1, 2, 3, 4, 5];
    for (let n of nums) {
        console.log(n * n);
    }

    return 0;
}
```

**Compiles to:**
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <format>

int main() {
    std::string name = "World";
    std::cout << std::format("Hello, {}!", name) << std::endl;

    std::vector<int> nums = {1, 2, 3, 4, 5};
    for (auto& n : nums) {
        std::cout << n * n << std::endl;
    }

    return 0;
}
```

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
jspp input.jspp -o output.cpp         # Compile to C++
jspp input.jspp -o output.cpp --run   # Compile + build + run
jspp input.jspp --emit-tokens         # Debug: print tokens
jspp input.jspp --emit-ast            # Debug: print AST
```

## JSPP Ship

`jspp-ship` is a minimal native REPL window built with sokol. It opens a single window with a render area on top and a text REPL on the bottom.

- The Ship is additive: it does not modify the existing compiler or prototype interpreter.
- The Ship runs JSPP through a persistent Node host process built on top of the prototype interpreter.
- REPL state is preserved in that host process, so variables, functions, classes, and retained draw calls survive between submissions without replaying the full session each time.
- The Ship requires Node.js to be installed and available when you run it.

Build the new target like any other CMake target:

```bash
cmake -S . -B build
cmake --build build --target jspp-ship
```

Current REPL controls:

- `Enter` inserts a newline
- `Ctrl+Enter` or `F5` executes the current snippet
- `Left` / `Right`, `Home`, `End`, `Backspace`, `Delete`, and `Tab` edit the current multiline buffer
- `Up` / `Down` move within multiline input and fall back to submitted-history recall at the top or bottom edge
- `Shift` + arrows extends the current selection, `Ctrl+A` selects the full buffer, and mouse drag selects text directly
- `Ctrl+Z` / `Ctrl+Y` undo and redo input edits
- `Ctrl+C` / `Ctrl+X` / `Ctrl+V` copy, cut, and paste the current selection

Built-in drawing helpers available in the Ship REPL include `drawRect(...)`, `drawCircle(...)`, `drawLine(...)`, and `clear()`. Calling `clear()` without arguments clears shapes while keeping the current render background color.

Built-in Ship self-test:

```bash
cmake --build build --target jspp-ship-selftest
```

This launches the Ship executable in self-test mode and writes a report to `build/jspp-ship-selftest.txt`.
The report now includes editor behavior, persistent-host execution, and live framebuffer pixel checks for draw colors, clear behavior, and render-area clipping.

## Key Features

- **JavaScript syntax** — `let`, `const`, `function`, arrow functions, classes
- **C++ performance** — stack allocation, zero GC, native binary output
- **Static types** — type inference with optional annotations
- **C++ interop** — `cpp { }` blocks for raw C++ anywhere
- **AI-friendly** — designed for AI code generation

## Documentation

- [Language Spec](docs/LANGUAGE_SPEC.md)
- [Token Definitions](docs/TOKENS.md)
- [Grammar (EBNF)](docs/GRAMMAR.md)
- [Architecture](docs/ARCHITECTURE.md)
