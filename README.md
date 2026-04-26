# Nova Language Compiler

![Build](https://github.com/pchrysostomou/nova-lang/actions/workflows/build.yml/badge.svg)
![Tests](https://img.shields.io/badge/tests-276%2F276-brightgreen)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-MIT-orange)

A complete compiled programming language built from scratch in C++17 — no external dependencies, no LLVM, no magic. Every phase hand-written: tokenizer, parser, semantic analyzer, bytecode compiler, and virtual machine.

---

## The Language

Nova is a statically-typed language with clean syntax and C-style braces.

```nova
fn add(a: int, b: int) -> int {
    return a + b
}

let result = add(3, 5)
print(result)  // 8
```

```nova
fn fact(n: int) -> int {
    if (n == 0) { return 1 }
    else { return n * fact(n - 1) }
}

print(fact(10))  // 3628800
```

```nova
// For loops
let sum = 0
for (let i = 1; i <= 100; i = i + 1) {
    sum = sum + i
}
print(sum)  // 5050
```

```nova
// String concatenation
fn greet(name: string) -> string {
    return "Hello, " + name + "!"
}

print(greet("Nova"))  // Hello, Nova!
```

```nova
// FizzBuzz using modulo
fn fizzbuzz(n: int) {
    let i = 1
    while (i <= n) {
        if (i % 15 == 0) { print("FizzBuzz") }
        else {
            if (i % 3 == 0) { print("Fizz") }
            else {
                if (i % 5 == 0) { print("Buzz") }
                else { print(i) }
            }
        }
        i = i + 1
    }
}
fizzbuzz(20)
```

### Feature Set

| Feature | Status |
|---|---|
| Variables (`let`) | ✅ |
| Functions (`fn`) | ✅ |
| Recursion | ✅ |
| If / else | ✅ |
| While loops | ✅ |
| For loops | ✅ |
| Arithmetic (`+ - * / %`) | ✅ |
| String concatenation (`+`) | ✅ |
| Comparisons (`== != < > <= >=`) | ✅ |
| Booleans (`true` / `false`) | ✅ |
| Strings | ✅ |
| Type annotations | ✅ |
| `print` builtin | ✅ |
| Semantic analysis | ✅ |
| CLI driver (`nova file.nova`) | ✅ |

---

## Architecture

Nova source code passes through five phases before it runs:

```
  source.nova
      │
      ▼
  ┌─────────┐
  │  Lexer  │  Characters → Tokens
  └────┬────┘  "let x = 42" → [LET][IDENT:"x"][EQUALS][NUMBER:42]
       │
       ▼
  ┌─────────┐
  │ Parser  │  Tokens → AST (Abstract Syntax Tree)
  └────┬────┘  Recursive descent, 9-level operator precedence
       │
       ▼
  ┌──────────┐
  │ Analyzer │  AST → Validated AST
  └────┬─────┘  Checks: undefined vars, type mismatches,
       │               duplicate declarations, missing returns
       ▼
  ┌─────────┐
  │ CodeGen │  AST → Bytecode (Chunks)
  └────┬────┘  One Chunk per function + __main__ for top-level code
       │
       ▼
  ┌────────┐
  │   VM   │  Bytecode → Output
  └────────┘  Stack-based, frame-per-call, named locals
```

### Phase Details

**Phase 1 — Lexer** (`src/lexer.h`, `src/lexer.cpp`)
Reads source character by character and produces a flat list of tokens. Handles keywords (`let`, `fn`, `if`, `else`, `while`, `for`, `return`, `true`, `false`), identifiers, numbers (int and float), strings, operators (`->`, `==`, `!=`, `<=`, `>=`, `%`), and skips whitespace and `//` comments. Tracks line/column for error reporting.

**Phase 2 — Parser** (`src/parser.h`, `src/parser.cpp`, `src/ast.h`)
Recursive descent parser that builds an AST from tokens. Implements full operator precedence via a 9-level call chain: `Assignment → Equality → Comparison → Addition → Multiplication → Unary → Call → Primary`. Handles `let`, `fn`, `return`, `if/else`, `while`, `for`, and expression statements.

**Phase 3 — Semantic Analyzer** (`src/analyzer.h`, `src/analyzer.cpp`)
Two-pass analysis: first collects all function signatures (enabling forward calls), then walks the AST checking for:
- Undefined variables and functions
- Type mismatches (declarations, assignments, return types, binary ops)
- Duplicate variable and function declarations
- Missing return statements (full control-flow path analysis)

Uses a scope stack for nested block scoping (for loop init variables are properly scoped) and a separate function registry.

**Phase 4 — Code Generator** (`src/codegen.h`, `src/codegen.cpp`)
Walks the AST and emits bytecode into `Chunk` objects — one per function, plus `__main__` for top-level code. Handles jump patching for if/else, while, and for loops. For loops desugar to an init statement followed by a while-style loop with a trailing update step. Supports 19 opcodes: `PUSH_CONST`, `LOAD`, `STORE`, `ADD/SUB/MUL/DIV/MOD`, `NEG/NOT`, comparison ops, `JUMP/JUMP_IF_FALSE`, `CALL/RETURN/RETURN_VAL`, `PRINT`, `POP/DUP`, `HALT`.

**Phase 5 — Virtual Machine** (`src/vm.h`, `src/vm.cpp`)
Stack-based VM with a per-call frame stack. Each frame holds a `locals` map. `LOAD` searches from innermost to outermost frame (enabling global variable access from functions). `STORE` always writes to the current frame. The `ADD` opcode handles both numeric addition and string concatenation. Built-in `print` is handled inline in the `CALL` dispatcher. Division-by-zero and undefined variable errors are caught at runtime.

---

## Test Results

```
Phase 1  —  Lexer       46 /  46  tests passing
Phase 2  —  Parser     103 / 103  tests passing
Phase 3  —  Analyzer    61 /  61  tests passing
Phase 4  —  CodeGen     66 /  66  tests passing
────────────────────────────────────────────────
Total                  276 / 276  ALL PASSING
```

---

## Build

### Prerequisites

**Windows — MSYS2 UCRT64:**

```powershell
winget install MSYS2.MSYS2
```

Then open **MSYS2 UCRT64** and install the toolchain:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake --noconfirm
```

Add `C:\msys64\ucrt64\bin` to your system `PATH`.

**Linux / macOS:**

```bash
# Ubuntu/Debian
sudo apt install g++ cmake make

# macOS
brew install cmake  # g++ comes with Xcode Command Line Tools
```

### Compile

```bash
git clone https://github.com/pchrysostomou/nova-lang
cd novalang
cmake -S . -B build
cmake --build build --parallel
```

### Run all tests

```bash
./build/test_lexer
./build/test_parser
./build/test_analyzer
./build/test_codegen
```

---

## Running Nova Programs

```bash
./build/nova programs/factorial.nova
./build/nova programs/fizzbuzz.nova
./build/nova programs/strings.nova
./build/nova programs/counting.nova
```

Or write your own `.nova` file:

```nova
// hello.nova
fn greet(name: string) -> string {
    return "Hello, " + name + "!"
}

for (let i = 0; i < 3; i = i + 1) {
    print(greet("World"))
}
```

```bash
./build/nova hello.nova
# Hello, World!
# Hello, World!
# Hello, World!
```

---

## Project Structure

```
novalang/
├── src/
│   ├── lexer.h / lexer.cpp         Phase 1 — Tokenizer
│   ├── ast.h                        Phase 2 — AST node definitions
│   ├── parser.h / parser.cpp        Phase 2 — Recursive descent parser
│   ├── analyzer.h / analyzer.cpp    Phase 3 — Semantic analysis
│   ├── vm.h / vm.cpp                Phase 4 — Bytecode VM & value types
│   ├── codegen.h / codegen.cpp      Phase 4 — AST → Bytecode compiler
│   └── main.cpp                     CLI driver (nova <file.nova>)
│
├── tests/
│   ├── test_lexer.cpp               46  tests
│   ├── test_parser.cpp             103  tests
│   ├── test_analyzer.cpp            61  tests
│   └── test_codegen.cpp             66  tests
│
├── programs/
│   ├── hello.nova
│   ├── math.nova
│   ├── factorial.nova
│   ├── fizzbuzz.nova
│   ├── strings.nova
│   └── counting.nova
│
├── .github/workflows/build.yml      CI — builds and runs all tests on Ubuntu
├── CMakeLists.txt
└── README.md
```

---

## Tech Stack

| Component | Technology |
|---|---|
| Implementation language | C++17 |
| Build system | CMake 3.20+ |
| Compiler (Windows) | MinGW-w64 g++ 15 (MSYS2 UCRT64) |
| Compiler (Linux/macOS) | g++ / clang++ |
| CI | GitHub Actions (ubuntu-latest) |
| Testing | Hand-written test runner (no external framework) |
| Dependencies | None |

---

## Roadmap

- [x] Lexer, Parser, Semantic Analyzer
- [x] Bytecode VM and Code Generator
- [x] CLI driver (`nova file.nova`)
- [x] Modulo operator (`%`)
- [x] String concatenation (`+`)
- [x] For loops
- [ ] Arrays
- [ ] Standard library builtins (`len`, `str`, `int`)
- [ ] User-defined types (`struct`)
- [ ] LLVM backend — native machine code generation
