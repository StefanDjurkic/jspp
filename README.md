# JSPP — JavaScript syntax, compiled to C++

[![CI](https://github.com/StefanDjurkic/jspp/actions/workflows/ci.yml/badge.svg)](https://github.com/StefanDjurkic/jspp/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

JSPP is a statically-typed language that reads like modern JavaScript and compiles to C++. It ships with a native compiler (`lexer → parser → type checker → code generator → .cpp`), a Node.js reference interpreter for fast iteration, and a regression test suite.

- **Try it in your browser:** https://stefandjurkic.github.io/zeroengine/ — playground and sample gallery, running on the reference interpreter.
- **Run it natively:** [Download ZeroEngine Desktop](https://github.com/StefanDjurkic/zeroengine/releases/latest). Hit *Run as compiled C++* and your source is compiled through `jspp → .cpp → g++ / clang++ / cl → native binary`, with the binary's stdout streamed back to the canvas.

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

Generated C++:

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

## Features

- JS-like syntax: `let`, `const`, `function`, arrow functions, classes, `for…of`, destructuring, template literals.
- Static types with inference; optional explicit annotations.
- Zero-GC, stack-preferred codegen.
- `cpp { ... }` escape hatch for raw C++.
- Sizable JS-style standard library in the reference interpreter (`Math`, `JSON`, `Array` / `String` methods, `Date`, `RegExp`, plus lodash-style helpers, `EventEmitter`, `uuid`, `path`, and more — see `tests/` for the exercised surface).

## Build

Requires a C++20 compiler (MSVC, clang ≥ 14, gcc ≥ 12), CMake ≥ 3.20, and Node.js ≥ 20 for the interpreter and tests.

```bash
cmake -S . -B build
cmake --build build
```

The binary is `build/jspp` (or `build/Debug/jspp.exe` on Windows).

## Usage

```bash
jspp input.jspp -o output.cpp         # Compile to C++
jspp input.jspp -o output.cpp --run   # Compile, build, run
jspp input.jspp --emit-tokens         # Debug: token stream
jspp input.jspp --emit-ast            # Debug: AST

jspp init my-project                  # Scaffold a project
```

`jspp init` creates `src/main.jspp`, a `jspp.toml` manifest, `tests/`, a `.gitignore`, and a GitHub Actions workflow. Pass `--bare` for just `src/` and `jspp.toml`.

You can also run `.jspp` files directly on Node without a C++ build:

```bash
node prototype/jspp.mjs examples/hello.jspp --run
```

## Tests

```bash
node tests/run.mjs
```

Each test is a `tests/NN_name.jspp` + `tests/NN_name.expected` pair; the runner executes the source through the interpreter and diffs stdout.

## Native REPL (`jspp-ship`)

```bash
cmake --build build --target jspp-ship
```

A single-window REPL built on [sokol](https://github.com/floooh/sokol) with a render area on top and a text buffer on the bottom. `Ctrl+Enter` / `F5` executes the current snippet; draw helpers `drawRect`, `drawCircle`, `drawLine`, `clear` are pre-bound. Standard editing, selection, clipboard, and undo/redo shortcuts work. A headless self-test target (`jspp-ship-selftest`) produces a framebuffer report at `build/jspp-ship-selftest.txt`.

## About the ZeroEngine benchmark

The playground in [ZeroEngine Desktop](https://github.com/StefanDjurkic/zeroengine) has a **Benchmark** button that runs your program through both the JS reference interpreter and the native C++ pipeline and reports per-frame cost. The result is often counter-intuitive — on draw-heavy workloads the native column can look *slower* than the interpreter. This is the benchmark's methodology, not a compiler problem:

- The JS column stubs every draw builtin to a no-op, so it only measures `tick()` math.
- The C++ column has to serialize every draw into the ZeroEngine stdout protocol (`@C`, `@R`, `@O`, `@L`, …) so the host can replay frames on canvas. That is roughly 3.5 MB of formatted output per 240-frame run for a 400-particle scene.

For compute alone, compiled C++ is typically **30–100×** faster than the tree-walking interpreter. The difference shrinks — and can invert — once you add a per-frame pipe-write tax that the interpreter does not pay. In a real engine binding `drawCircle(x, y, r)` would be a direct call into the renderer with no serialization, and the compute advantage returns in full.

The generated `main()` already does what it can: `sync_with_stdio(false)` plus `cout.tie(nullptr)`, buffered `snprintf` into a thread-local `std::string`, and one `fwrite` at end-of-run. For a pure-compute comparison without the I/O asymmetry, see the compile-pipeline demos in [`zeroengine/demos/compiled/`](https://github.com/StefanDjurkic/zeroengine/tree/main/demos/compiled).

## Project layout

```
src/         C++ compiler sources (lexer, parser, typechecker, codegen)
prototype/   Node.js reference interpreter + type checker
examples/    Sample .jspp programs
tests/       Regression test suite (run.mjs)
docs/        Language spec, grammar, token list, architecture notes
ship/        Native REPL (sokol)
third_party/ Vendored sokol headers
```

## Editor support

[jspp-vscode](https://github.com/StefanDjurkic/jspp-vscode) — syntax highlighting, snippets, language configuration, and embedded C++ highlighting inside `cpp { ... }` blocks.

```bash
# Grab the latest .vsix from:
# https://github.com/StefanDjurkic/jspp-vscode/releases
code --install-extension jspp-<version>.vsix
```

## Documentation

- [Language spec](docs/LANGUAGE_SPEC.md)
- [Tokens](docs/TOKENS.md)
- [Grammar (EBNF)](docs/GRAMMAR.md)
- [Architecture](docs/ARCHITECTURE.md)

## Contributing

Side project; contributions welcome. Open an issue or PR.

## License

MIT — see [LICENSE](LICENSE). Third-party code under `third_party/` keeps its original license.
