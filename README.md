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
# JSPP (JavaScript++) — JavaScript syntax, C++ performance? (maybe sometimes) 

[![CI](https://github.com/StefanDjurkic/jspp/actions/workflows/ci.yml/badge.svg)](https://github.com/StefanDjurkic/jspp/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**🎮 Try it in your browser:** https://stefandjurkic.github.io/zeroengine/ — the [ZeroEngine](https://github.com/StefanDjurkic/zeroengine) home page, playground, and sample-app gallery, all running JSPP live.

**🖥️ Run it natively:** [Download ZeroEngine Desktop](https://github.com/StefanDjurkic/zeroengine/releases/latest) — ships this compiler next to the app. Hit **Run as compiled C++** in the playground and your JSPP is compiled through `jspp → .cpp → g++/clang++/cl → native .exe` and the binary's stdout is streamed back to the canvas. The desktop app also opens a local HTTP bridge (`127.0.0.1:17849`) so the public website lights up when it's running and can send compile jobs to your machine.

## Why JSPP?

AI writes better JavaScript than C++. There's more JS in every training set, the syntax is more regular, and there are fewer footguns — no manual memory management, no header files, no template metaprogramming. JSPP exploits that gap: give the AI a language it already knows how to write, and let the compiler handle the hard translation to native code. The AI stays in its comfort zone. You still get C++ speed.

JSPP is a statically-typed language that reads like modern JavaScript and compiles to high-performance C++. It ships with:

- A **C++ compiler** (lexer → parser → type checker → code generator → `.cpp` output).
- A **JavaScript reference interpreter** (`prototype/jspp.mjs`) that runs `.jspp` files directly under Node — used for fast iteration, the stdlib, and the test suite.
- A native **REPL window** (`jspp-ship`) built on top of [sokol](https://github.com/floooh/sokol).
- A **regression test suite** covering variables, functions, control flow, arrays, objects, closures, classes, switch/enums, arrow functions, `for…of`, try/catch, imports, and a broad JavaScript-style standard library (Math, JSON, Array/String methods, Date, RegExp, lodash-style helpers, EventEmitter, uuid, path, date-fns, query-string, ms, invariant/warning/assert, etc.).
- **Integration with [ZeroEngine Desktop](https://github.com/StefanDjurkic/zeroengine)**: a Tauri shell that combines the playground, a sample-app gallery (bouncy balls, pendulum, particle field, 3D cube, …), and a local compile-and-run bridge. Inside the playground a **Benchmark** button runs your current program both through the JS reference interpreter and as compiled native C++ and shows the per-frame speedup. See [What the ZeroEngine benchmark is actually measuring](#what-the-zeroengine-benchmark-is-actually-measuring) below before reading too much into the numbers.

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

## Start a new project

```bash
jspp init my-project
cd my-project
jspp src/main.jspp -o main.cpp
c++ -std=c++20 main.cpp -o my-project && ./my-project
```

`jspp init` scaffolds a ready-to-build project with `src/main.jspp`, a `jspp.toml` manifest, `tests/`, a `.gitignore`, a starter `README.md`, and a GitHub Actions workflow. Pass `--bare` for a minimal layout (just `src/` and `jspp.toml`).

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

## What the ZeroEngine benchmark is actually measuring

The **Benchmark** button in the playground runs your program two ways and prints a per-frame cost for each. On workloads with many draw calls (particle field, pendulum, starfield), the native C++ number often looks *slower* than the JS interpreter number. That is not a JSPP bug — it is what the benchmark is physically measuring, and it is worth spelling out because the result is counter-intuitive.

**The two columns do different amounts of work:**

| | JS interpreter column | C++ column |
|---|---|---|
| Runs your `tick()` math | ✅ | ✅ |
| Calls `drawCircle(...)` N×/frame | ✅ (but **stubbed to no-op**) | ✅ (must **serialize the draw command to stdout**) |
| Writes bytes to a pipe and has the host process read them | ❌ | ✅ (~15 KB/frame for 400 particles) |

The JS side never touches a canvas during the benchmark — the draw builtins are replaced with empty functions so it only measures your program logic. The C++ side actually writes the ZeroEngine visual protocol (`@C`, `@R`, `@O`, `@L`, …) to stdout, because that's how frames get streamed back through the Tauri bridge to be replayed on the canvas. For a 400-particle scene that's ~2800 floating-point numbers formatted per frame, pushed through a Windows pipe, read back by the host.

**What this means in practice:**

- **Pure `tick()` math** in compiled C++ is typically **30–100× faster** than the same code in JSPP's tree-walking interpreter.
- **End-to-end "tick() + serialize every draw to stdout"** is dominated by I/O on any workload that draws a lot per frame. On pendulum (two draws/frame) and the 3D cube (three per-face color writes) the compute advantage of native code is essentially rounded off by per-frame pipe writes, so native looks similar to or slower than the interpreter.
- Particle field (400 draws/frame, ~3.5 MB of stdout) is still I/O-bound but has enough compute in `tick()` that you start to see the compiled code pull ahead.

**Optimizations already applied to the generated C++ for this workload:**

- `std::ios::sync_with_stdio(false); std::cout.tie(nullptr);` in the synthesized `main()` so `std::cout` stops sharing a buffer with the C library's `stdout`.
- Every draw builtin buffers its output into a thread-local `std::string` via `snprintf` instead of going through `operator<<` formatting and per-call locks.
- The whole frame is flushed with a single `fwrite` at end-of-run instead of per-frame flushes, collapsing hundreds of syscalls into one.

**What the benchmark is *not* a good proxy for:**

- It is **not** a measurement of JSPP-as-a-game-engine-scripting-language. In a real engine binding, `drawCircle(x, y, r)` is a direct call into the renderer — no serialization, no pipe, no parsing. That architecture is several orders of magnitude faster than either column of this benchmark.
- It is **not** a comparison to hand-written JavaScript in V8. JSPP's reference interpreter is a tree-walker with no JIT; V8 would beat it badly on the same source. The honest claim here is "compiled JSPP ≫ interpreted JSPP for compute."

If you want to feel the native advantage without the I/O asymmetry, run the compile-pipeline demos in [`zeroengine/demos/compiled/`](https://github.com/StefanDjurkic/zeroengine/tree/main/demos/compiled) directly — those are pure-compute programs whose stdout is a one-shot result, not a frame protocol, and the per-iteration cost is whatever the C++ compiler would give you for equivalent handwritten code.

## Optional: ZeroEngine browser target

JSPP can also run inside a WebGPU browser host called **[ZeroEngine](https://github.com/StefanDjurkic/zeroengine)**, which exposes drawing builtins (`drawRect`, `drawCircle`, `drawLine`, `clear`) to `.jspp` programs and provides a Playwright-based browser regression harness. ZeroEngine is a **separate, optional project** and is not required to build, run, or test JSPP.

If you want to try it, clone it as a **sibling** of this repo so its test harness can find `tests/run.mjs`:

```
your-workspace/
├── jspp/          # this repo
└── zeroengine/    # optional, separate repo — https://github.com/StefanDjurkic/zeroengine
```

See the [ZeroEngine repository](https://github.com/StefanDjurkic/zeroengine) for setup instructions.

## Demos

Three complementary surfaces demonstrate JSPP:

### 1. Live cube on the landing page — https://stefandjurkic.github.io/zeroengine/

A JSPP script drives a Three.js cube in real time. Rotation, scale, per-face colors, and click behavior are all written in JSPP; the JS reference interpreter (`prototype/jspp.mjs`) executes that source in the browser and calls into Three.js to render.

### 2. Browser playground — https://stefandjurkic.github.io/zeroengine/jspp.html

A real editor with six scene pills, all running through the reference interpreter:

| Scene | What it does |
|---|---|
| `hello` | Paints a random composition of rectangles and circles on a 2D canvas. |
| `loops` | Nested `for` loops tile a hue-shifted grid of circles. |
| `functions` | Recursive Fibonacci rendered as a bar chart. |
| `classes` | A `Ball` class with fields and methods, several instances drawn. |
| `bouncing` | User-defined `tick(t)` animates balls bouncing off canvas walls. |
| `3D cube` | JSPP `tick(dt)` drives a real Three.js cube via `setRotation` / `setScale` / `setFaceColor`. |

The playground also has a **View compiled C++** button that opens a viewer over the committed compile-pipeline artifacts below.

### 3. Compile-pipeline demos — [`zeroengine/demos/compiled/`](https://github.com/StefanDjurkic/zeroengine/tree/main/demos/compiled)

Four JSPP programs round-tripped through the **full** `jspp -> .cpp -> native binary` pipeline. Each folder commits `source.jspp`, `generated.cpp` (what this compiler emitted, verbatim), and `expected.txt` (what the native binary actually printed). ZeroEngine's `jspp-pipeline` CI job builds this compiler from source and re-runs the whole pipeline on every push.

The same `generated.cpp` is **also compiled to WebAssembly** via Emscripten. The playground's modal has a **run wasm** tab that imports the emcc-built `demo.mjs` in your browser, calls `main()`, captures stdout, and live-diffs it against `expected.txt`. Same C++, two backends (g++ native and emcc wasm), identical stdout - verified in CI.

| Demo | What it does |
|---|---|
| `hello` | `print("Hello, World!")` - the smallest end-to-end pipeline test. |
| `fibonacci` | Recursive `fib(n)` for n = 0..9. Exercises functions, recursion, integer codegen. |
| `classes` | `Player` class with `health`, `name`, and a `takeDamage(amount)` method; two instances, state mutation. |
| `demo` | Broader mix: variables, arithmetic, control flow, string concat - more of the compiler surface in one program. |
| `mandelbrot` | A 96x64 Mandelbrot: `main()` prints W*H iteration counts that the playground paints as a fractal - every pixel is computed by C++-to-wasm from this JSPP source. |

## Editor support

A VS Code extension is available: **[jspp-vscode](https://github.com/StefanDjurkic/jspp-vscode)** — syntax highlighting, snippets, and language configuration for `.jspp` files, including embedded C++ highlighting inside `cpp { ... }` blocks.

Install the latest release:

```bash
# Download jspp-<version>.vsix from:
# https://github.com/StefanDjurkic/jspp-vscode/releases
code --install-extension jspp-<version>.vsix
```

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
