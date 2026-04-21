# JSPP Language Specification v0.1

## 1. Overview

JSPP is a statically-typed, compiled language with JavaScript syntax that produces native machine code via C++ emission. It is designed so that code written in familiar JS style compiles into efficient, readable, modern C++ (C++20+).

**Design philosophy:**
- If you can write JavaScript, you can write JSPP.
- The output is real C++ — no runtime, no GC, no interpreter.
- You can drop into raw C++ at any time via `cpp { }` blocks.
- Type inference makes code concise; explicit annotations are always available.
- Memory is stack-allocated by default; heap via explicit `new` / smart pointers.

---

## 2. Source Files

- Extension: `.jspp`
- Encoding: UTF-8
- Entry point: `function main()` (maps to `int main()`)
- Each `.jspp` file compiles to one `.cpp` / `.hpp` pair.

---

## 3. Primitive Types

| JSPP Type  | C++ Mapping         | Description                    |
|------------|----------------------|--------------------------------|
| `int`      | `int`               | 32-bit signed integer          |
| `i8`       | `int8_t`            | 8-bit signed integer           |
| `i16`      | `int16_t`           | 16-bit signed integer          |
| `i32`      | `int32_t`           | 32-bit signed integer          |
| `i64`      | `int64_t`           | 64-bit signed integer          |
| `u8`       | `uint8_t`           | 8-bit unsigned integer         |
| `u16`      | `uint16_t`          | 16-bit unsigned integer        |
| `u32`      | `uint32_t`          | 32-bit unsigned integer        |
| `u64`      | `uint64_t`          | 64-bit unsigned integer        |
| `float`    | `float`             | 32-bit IEEE 754                |
| `double`   | `double`            | 64-bit IEEE 754                |
| `bool`     | `bool`              | true / false                   |
| `string`   | `std::string`       | UTF-8 string                   |
| `char`     | `char`              | Single byte character          |
| `void`     | `void`              | No return value                |
| `nullptr`  | `nullptr`           | Null pointer literal           |
| `auto`     | `auto`              | Compiler-inferred type         |
| `size`     | `size_t`            | Platform-sized unsigned int    |

---

## 4. Variable Declarations

### 4.1 `let` — mutable variable
```jspp
let x: int = 10;
let y = 5;          // inferred as int
let name = "hello"; // inferred as std::string
```
**C++ output:**
```cpp
int x = 10;
int y = 5;
std::string name = "hello";
```

### 4.2 `const` — immutable variable
```jspp
const PI: double = 3.14159;
const msg = "hi";
```
**C++ output:**
```cpp
const double PI = 3.14159;
const std::string msg = "hi";
```

### 4.3 Type inference rules
- Integer literals → `int`
- Floating literals (with `.`) → `double`
- String literals (`"..."`) → `std::string`
- Character literals (`'c'`) → `char`
- Boolean literals (`true`/`false`) → `bool`
- Array literals → `std::vector<T>` where T is inferred from elements
- Object literals → anonymous struct or named struct

---

## 5. Functions

### 5.1 Named functions
```jspp
function add(a: int, b: int): int {
    return a + b;
}
```
**C++ output:**
```cpp
int add(int a, int b) {
    return a + b;
}
```

### 5.2 Arrow functions (lambdas)
```jspp
const square = (x: int): int => x * x;
const greet = (name: string): string => "Hello " + name;
```
**C++ output:**
```cpp
auto square = [](int x) -> int { return x * x; };
auto greet = [](std::string name) -> std::string { return "Hello " + name; };
```

### 5.3 Arrow functions with body
```jspp
const compute = (a: int, b: int): int => {
    let result = a * b + 1;
    return result;
};
```
**C++ output:**
```cpp
auto compute = [](int a, int b) -> int {
    int result = a * b + 1;
    return result;
};
```

### 5.4 Default parameters
```jspp
function greet(name: string, greeting: string = "Hello"): string {
    return greeting + ", " + name;
}
```
**C++ output:**
```cpp
std::string greet(std::string name, std::string greeting = "Hello") {
    return greeting + ", " + name;
}
```

### 5.5 Void functions
```jspp
function log(msg: string): void {
    console.log(msg); // maps to std::cout
}
```

### 5.6 Main entry point
```jspp
function main(): int {
    return 0;
}
```
**C++ output:**
```cpp
int main() {
    return 0;
}
```

---

## 6. Objects → Structs

### 6.1 Inline object literals
```jspp
let p = { x: 10, y: 20 };
```
**C++ output:**
```cpp
struct __anon_p { int x; int y; };
__anon_p p = {10, 20};
```

### 6.2 Named struct declarations (`type` keyword)
```jspp
type Point = {
    x: double;
    y: double;
};

let origin: Point = { x: 0.0, y: 0.0 };
```
**C++ output:**
```cpp
struct Point {
    double x;
    double y;
};

Point origin = {0.0, 0.0};
```

### 6.3 Struct methods
```jspp
type Vector2 = {
    x: double;
    y: double;

    function length(): double {
        return Math.sqrt(this.x * this.x + this.y * this.y);
    }
};
```
**C++ output:**
```cpp
struct Vector2 {
    double x;
    double y;

    double length() {
        return std::sqrt(this->x * this->x + this->y * this->y);
    }
};
```

---

## 7. Classes

JSPP supports classes with a JS-like syntax that maps to C++ classes.

```jspp
class Entity {
    name: string;
    health: int;

    constructor(name: string, health: int) {
        this.name = name;
        this.health = health;
    }

    function takeDamage(amount: int): void {
        this.health -= amount;
    }

    function isAlive(): bool {
        return this.health > 0;
    }
}
```
**C++ output:**
```cpp
class Entity {
public:
    std::string name;
    int health;

    Entity(std::string name, int health) : name(name), health(health) {}

    void takeDamage(int amount) {
        this->health -= amount;
    }

    bool isAlive() {
        return this->health > 0;
    }
};
```

### 7.1 Inheritance
```jspp
class Player extends Entity {
    score: int = 0;

    constructor(name: string) {
        super(name, 100);
    }
}
```

---

## 8. Arrays → std::vector

```jspp
let nums: int[] = [1, 2, 3, 4, 5];
let names = ["Alice", "Bob"]; // inferred string[]

nums.push(6);          // push_back
let len = nums.length; // .size()
let first = nums[0];
```
**C++ output:**
```cpp
std::vector<int> nums = {1, 2, 3, 4, 5};
std::vector<std::string> names = {"Alice", "Bob"};

nums.push_back(6);
auto len = nums.size();
auto first = nums[0];
```

### 8.1 Array methods mapping
| JSPP               | C++ Mapping                              |
|---------------------|------------------------------------------|
| `arr.push(val)`    | `arr.push_back(val)`                     |
| `arr.pop()`        | `arr.pop_back()`                         |
| `arr.length`       | `arr.size()`                             |
| `arr.at(i)`        | `arr.at(i)`                              |
| `arr[i]`           | `arr[i]`                                 |
| `arr.includes(v)`  | `std::find(begin,end,v) != end`          |
| `arr.indexOf(v)`   | `std::distance(begin, std::find(...))`   |

---

## 9. Control Flow

### 9.1 If / else
```jspp
if (x > 0) {
    // ...
} else if (x == 0) {
    // ...
} else {
    // ...
}
```
Maps 1:1 to C++.

### 9.2 For loops
```jspp
for (let i = 0; i < 10; i++) {
    // ...
}
```
**C++ output:**
```cpp
for (int i = 0; i < 10; i++) {
    // ...
}
```

### 9.3 For-of loops (range-based)
```jspp
for (let item of items) {
    // ...
}
```
**C++ output:**
```cpp
for (auto& item : items) {
    // ...
}
```

### 9.4 While / do-while
```jspp
while (condition) { }
do { } while (condition);
```
Direct 1:1 mapping.

### 9.5 Switch
```jspp
switch (val) {
    case 1:
        break;
    case 2:
        break;
    default:
        break;
}
```
Direct 1:1 mapping.

---

## 10. Console / IO

### 10.1 Output
```jspp
console.log("Hello, World!");
console.log("x =", x);
```
**C++ output:**
```cpp
std::cout << "Hello, World!" << std::endl;
std::cout << "x =" << " " << x << std::endl;
```

### 10.2 Input
```jspp
let input: string = console.read();
let num: int = console.readInt();
```
**C++ output:**
```cpp
std::string input;
std::getline(std::cin, input);
int num;
std::cin >> num;
```

---

## 11. String Operations

```jspp
let s = "Hello";
let len = s.length;           // .size()
let upper = s.toUpperCase();  // custom helper or boost
let sub = s.slice(0, 3);      // .substr(0, 3)
let has = s.includes("ell");   // .find() != npos
let idx = s.indexOf("lo");    // .find()
let combined = s + " World";  // operator+
```

---

## 12. Memory Model

### 12.1 Stack by default
All `let` and `const` declarations are stack-allocated.

### 12.2 Heap allocation
```jspp
let p = new Point(1.0, 2.0);  // unique_ptr by default
let shared = shared_new Point(1.0, 2.0); // shared_ptr
```
**C++ output:**
```cpp
auto p = std::make_unique<Point>(1.0, 2.0);
auto shared = std::make_shared<Point>(1.0, 2.0);
```

### 12.3 References
```jspp
function modify(ref p: Point): void {
    p.x = 100;
}
```
**C++ output:**
```cpp
void modify(Point& p) {
    p.x = 100;
}
```

### 12.4 Pointers (opt-in, for C++ interop)
```jspp
let raw: ptr<int> = nullptr;
```
**C++ output:**
```cpp
int* raw = nullptr;
```

---

## 13. C++ Interop — `cpp { }` Blocks

You can drop into raw C++ anywhere:

```jspp
function fastCompute(data: int[]): int {
    cpp {
        int sum = 0;
        for (auto& d : data) sum += d;
        return sum;
    }
}
```

The `cpp { }` block is emitted verbatim into the output.

### 13.1 Include directives
```jspp
cpp_include "<algorithm>";
cpp_include "<numeric>";
cpp_include "\"my_header.h\"";
```
**C++ output:**
```cpp
#include <algorithm>
#include <numeric>
#include "my_header.h"
```

---

## 14. Imports / Modules

```jspp
import { Point, Vector2 } from "./geometry";
import * as math from "./mathlib";
```
**C++ output:**
```cpp
#include "geometry.hpp"
#include "mathlib.hpp"
```
Each `.jspp` file generates a `.hpp` header and `.cpp` implementation.

---

## 15. Templates / Generics

```jspp
function identity<T>(val: T): T {
    return val;
}

type Container<T> = {
    data: T[];
    function add(item: T): void {
        this.data.push(item);
    }
};
```
**C++ output:**
```cpp
template<typename T>
T identity(T val) {
    return val;
}

template<typename T>
struct Container {
    std::vector<T> data;
    void add(T item) {
        data.push_back(item);
    }
};
```

---

## 16. Enums

```jspp
enum Color {
    Red,
    Green,
    Blue
}

enum Status {
    OK = 200,
    NotFound = 404
}
```
**C++ output:**
```cpp
enum class Color {
    Red,
    Green,
    Blue
};

enum class Status {
    OK = 200,
    NotFound = 404
};
```

---

## 17. Error Handling

### 17.1 try/catch with typed exceptions
```jspp
try {
    riskyOperation();
} catch (e: string) {
    console.log("Error:", e);
}
```
**C++ output:**
```cpp
try {
    riskyOperation();
} catch (const std::string& e) {
    std::cout << "Error:" << " " << e << std::endl;
}
```

### 17.2 throw
```jspp
throw "Something went wrong";
```
**C++ output:**
```cpp
throw std::string("Something went wrong");
```

---

## 18. Operator Overloading

Inside `type` or `class`:
```jspp
type Vec2 = {
    x: double;
    y: double;

    operator +(other: Vec2): Vec2 {
        return { x: this.x + other.x, y: this.y + other.y };
    }
};
```

---

## 19. Const Correctness

- `const` variables → `const` in C++
- `const` method params → `const T&` in C++ for non-primitive types
- String params automatically passed as `const std::string&` unless `ref` specified

---

## 20. Standard Library Mappings

| JSPP               | C++ Equivalent                       |
|---------------------|--------------------------------------|
| `Math.sqrt(x)`     | `std::sqrt(x)`                       |
| `Math.abs(x)`      | `std::abs(x)`                        |
| `Math.floor(x)`    | `std::floor(x)`                      |
| `Math.ceil(x)`     | `std::ceil(x)`                       |
| `Math.max(a, b)`   | `std::max(a, b)`                     |
| `Math.min(a, b)`   | `std::min(a, b)`                     |
| `Math.PI`          | `M_PI`                               |
| `Math.random()`    | `(rand() / (double)RAND_MAX)`        |
| `console.log()`    | `std::cout <<`                       |
| `console.read()`   | `std::getline(std::cin, ...)`        |

---

## 21. Forbidden JavaScript Features

The following JS features are **NOT** supported and will produce compile errors:

- `var` declarations
- `undefined`, `NaN` as values
- Prototype chains
- `typeof` operator (use compile-time type checks)
- Dynamic property access (`obj[key]` where key is runtime string)
- `eval()`
- `arguments` object
- `with` statement
- Implicit type coercion (`"5" + 3`)
- `==` (loose equality) — only `===` allowed (maps to `==` in C++)
- Spread into unknown types
- `any` type
- Runtime reflection

---

## 22. Comparison Operators

| JSPP    | C++ Output | Notes                           |
|---------|------------|----------------------------------|
| `===`   | `==`       | Strict equality (only option)    |
| `!==`   | `!=`       | Strict inequality                |
| `<`     | `<`        | Direct                           |
| `>`     | `>`        | Direct                           |
| `<=`    | `<=`       | Direct                           |
| `>=`    | `>=`       | Direct                           |

Note: `==` (loose) is forbidden. Use `===` only.

---

## 23. Null Safety

```jspp
let p: Point? = null;        // optional/nullable
if (p !== null) {
    console.log(p.x);
}
```
**C++ output:**
```cpp
std::optional<Point> p = std::nullopt;
if (p.has_value()) {
    std::cout << p->x << std::endl;
}
```

---

## 24. Destructuring

### 24.1 Object destructuring
```jspp
let { x, y } = point;
```
**C++ output:**
```cpp
auto x = point.x;
auto y = point.y;
```

### 24.2 Array destructuring
```jspp
let [first, second] = arr;
```
**C++ output:**
```cpp
auto first = arr[0];
auto second = arr[1];
```

---

## 25. Template Strings

```jspp
let msg = `Hello ${name}, you are ${age} years old`;
```
**C++ output (using fmt or std::format):**
```cpp
auto msg = std::format("Hello {}, you are {} years old", name, age);
```

---

## 26. Compilation Pipeline

```
  .jspp source
       │
       ▼
   ┌────────┐
   │  Lexer  │  → Token stream
   └────────┘
       │
       ▼
   ┌────────┐
   │ Parser  │  → AST
   └────────┘
       │
       ▼
   ┌──────────────┐
   │ Type Checker  │  → Typed AST
   └──────────────┘
       │
       ▼
   ┌──────────────┐
   │ Code Generator│  → .cpp / .hpp files
   └──────────────┘
       │
       ▼
   ┌────────┐
   │  g++   │  → native binary
   └────────┘
```

---

## 27. Optional Type Annotations (Full Inference Mode)

JSPP allows omitting **all** type annotations. When types are omitted, the compiler
infers them from context. This lets you paste JavaScript-like code and have it compile.

### 27.1 Parameters without types
```jspp
function add(a, b) {
    return a + b;
}
```
**C++ output (C++20 abbreviated function template):**
```cpp
auto add(auto a, auto b) {
    return a + b;
}
```

When parameter types are omitted, they compile to C++20 `auto` parameters, making the
function an implicit template. The compiler resolves concrete types at call sites.

For non-recursive functions, `auto` return type deduction works automatically.

### 27.2 Recursive functions without types
For recursive functions, the compiler infers concrete types from the base case and call sites:
```jspp
function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

factorial(5); // call site tells compiler n is int
```
**C++ output:**
```cpp
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}
```

**Inference rules for untyped parameters:**
- If called with an `int` literal → `int`
- If called with a `double` literal → `double`
- If called with a `string` literal → `std::string`
- If only used generically → `auto` (C++20 template)
- If recursive → must be resolvable from at least one call site

---

## 28. `print()` Built-in

`print()` is a built-in function that writes its arguments separated by spaces, followed
by a newline. It accepts any number of arguments of any printable type.

```jspp
print("Hello, World!");
print("x:", x, "y:", y);
print("sum:", a + b);
```
**C++ output:**
```cpp
// Emitted once at top of file when print() is used:
template<typename... Args>
void print(Args&&... args) {
    bool __first = true;
    ((std::cout << (__first ? "" : " ") << args, __first = false), ...);
    std::cout << std::endl;
}

// Call sites emit directly:
print("Hello, World!");
print("x:", x, "y:", y);
print("sum:", a + b);
```

`print()` coexists with `console.log()`. Both are available:
| JSPP              | C++ Mapping                         |
|-------------------|-------------------------------------|
| `print(a, b, c)` | variadic template (see above)       |
| `console.log(a)`  | `std::cout << a << std::endl`      |

---

## 29. Anonymous Function Expressions

Functions can be used as values without a name, using the `function` keyword inline.
This is in addition to arrow function syntax.

```jspp
let doubled = map([2, 4, 6], function(n) { return n * 2; });

let handler = function(event) {
    print("Got event:", event);
};
```
**C++ output:**
```cpp
auto doubled = map(std::vector<int>{2, 4, 6}, [](auto n) { return n * 2; });

auto handler = [](auto event) {
    print("Got event:", event);
};
```

Anonymous functions compile to C++ lambdas. Parameters without types become `auto`.

### 29.1 Named function expressions
A function expression may optionally have a name (for recursion within the expression):
```jspp
let fib = function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
};
```

---

## 30. Closures

Functions that capture variables from their enclosing scope are closures.
Captured variables are preserved for the lifetime of the closure.

```jspp
function makeCounter(start) {
    let n = start;
    return function() {
        n = n + 1;
        return n;
    };
}

let counter = makeCounter(3);
print("counter():", counter()); // 4
print("counter():", counter()); // 5
```
**C++ output:**
```cpp
auto makeCounter(auto start) {
    auto n = start;
    return [n]() mutable {
        n = n + 1;
        return n;
    };
}

auto counter = makeCounter(3);
print("counter():", counter());
print("counter():", counter());
```

### 30.1 Closure capture rules
| Capture type     | JSPP behavior                | C++ mapping           |
|------------------|------------------------------|-----------------------|
| Read-only        | variable only read           | `[x]` (by value)     |
| Mutated          | variable assigned in closure | `[x]() mutable`      |
| Multiple vars    | all referenced vars captured | `[a, b, c]() mutable`|
| Shared ownership | closure outlives creator     | `[n]() mutable`      |

The compiler analyzes which variables are captured and whether they are mutated
to emit the correct C++ capture list.

---

## 31. Braceless Control Flow

`if`, `for`, `while`, and `for-of` may use a single statement instead of a block:

```jspp
if (n <= 1) return 1;

for (let i = 0; i < 10; i++) print(i);

while (running) tick();
```
**C++ output:**
```cpp
if (n <= 1) return 1;

for (int i = 0; i < 10; i++) print(i);

while (running) tick();
```

This is valid in both JS and C++, so it maps directly.

---

## 32. Dynamic Array Index Assignment

Assigning to an array index beyond its current length automatically resizes the array:

```jspp
let out = [];
out[0] = 10;
out[3] = 40; // indices 1, 2 get default-initialized
```
**C++ output:**
```cpp
std::vector<int> out = {};
if (0 >= out.size()) out.resize(0 + 1);
out[0] = 10;
if (3 >= out.size()) out.resize(3 + 1);
out[3] = 40;
```

Or using a runtime helper:
```cpp
// Emitted once when dynamic index assignment is detected:
template<typename T>
void __jspp_set(std::vector<T>& v, size_t i, T val) {
    if (i >= v.size()) v.resize(i + 1);
    v[i] = std::move(val);
}

// Usage:
__jspp_set(out, 0, 10);
__jspp_set(out, 3, 40);
```

---

## 33. Higher-Order Functions

Functions are first-class values. They can be passed as arguments, returned from
other functions, and stored in variables.

```jspp
function map(array, fn) {
    let out = [];
    for (let i = 0; i < array.length; i = i + 1) {
        out[i] = fn(array[i]);
    }
    return out;
}
```
**C++ output:**
```cpp
template<typename T, typename F>
auto map(std::vector<T> array, F fn) {
    std::vector<decltype(fn(array[0]))> out;
    for (size_t i = 0; i < array.size(); i = i + 1) {
        if (i >= out.size()) out.resize(i + 1);
        out[i] = fn(array[i]);
    }
    return out;
}
```

When a function parameter is called like a function in the body, the compiler infers
it as a callable type (C++ template parameter with no constraint, or `std::function`
when stored).
