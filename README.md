# Nova Language Compiler

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Tests](https://img.shields.io/badge/tests-259%2F259-brightgreen)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![License](https://img.shields.io/badge/license-MIT-orange)

A complete compiled programming language built from scratch in C++20 — no external dependencies, no LLVM, no magic. Every phase hand-written: tokenizer, parser, semantic analyzer, bytecode compiler, and virtual machine.

---

## The Language

Nova is a statically-typed, expression-oriented language with clean Python-like syntax and C-style braces.

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
let i = 0
let sum = 0
while (i < 100) {
    sum = sum + i
    i = i + 1
}
print(sum)  // 4950
```

### Feature Set

| Feature | Status |
|---|---|
| Variables (`let`) | ✅ |
| Functions (`fn`) | ✅ |
| Recursion | ✅ |
| If / else | ✅ |
| While loops | ✅ |
| Arithmetic (`+ - * /`) | ✅ |
| Comparisons (`== != < > <= >=`) | ✅ |
| Booleans (`true` / `false`) | ✅ |
| Strings | ✅ |
| Type annotations | ✅ |
| `print` builtin | ✅ |
| Semantic analysis | ✅ |

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
Reads source character by character and produces a flat list of tokens. Handles keywords, identifiers, numbers (int and float), strings, operators (`->`, `==`, `!=`, `<=`, `>=`), and skips whitespace and `//` comments. Tracks line/column for error reporting.

**Phase 2 — Parser** (`src/parser.h`, `src/parser.cpp`, `src/ast.h`)
Recursive descent parser that builds an AST from tokens. Implements full operator precedence via a 9-level call chain: `Assignment → Equality → Comparison → Addition → Multiplication → Unary → Call → Primary`. Handles `let`, `fn`, `return`, `if/else`, `while`, and expression statements.

**Phase 3 — Semantic Analyzer** (`src/analyzer.h`, `src/analyzer.cpp`)
Two-pass analysis: first collects all function signatures (enabling forward calls), then walks the AST checking for:
- Undefined variables and functions
- Type mismatches (declarations, assignments, return types, binary ops)
- Duplicate variable and function declarations
- Missing return statements (full control-flow path analysis)

Uses a scope stack for nested block scoping and a separate function registry.

**Phase 4 — Code Generator** (`src/codegen.h`, `src/codegen.cpp`)
Walks the AST and emits bytecode into `Chunk` objects — one per function, plus `__main__` for top-level code. Handles jump patching for if/else and while loops. Supports 18 opcodes: `PUSH_CONST`, `LOAD`, `STORE`, `ADD/SUB/MUL/DIV`, `NEG/NOT`, comparison ops, `JUMP/JUMP_IF_FALSE`, `CALL/RETURN/RETURN_VAL`, `PRINT`, `POP/DUP`, `HALT`.

**Phase 5 — Virtual Machine** (`src/vm.h`, `src/vm.cpp`)
Stack-based VM with a per-call frame stack. Each frame holds a `locals` map. `LOAD` searches from innermost to outermost frame (enabling global variable access from functions). `STORE` always writes to the current frame. Built-in `print` is handled inline in the `CALL` dispatcher. Division-by-zero and undefined variable errors are caught at runtime.

---

## Test Results

```
Phase 1  —  Lexer       46 / 46   tests passing
Phase 2  —  Parser     103 / 103  tests passing
Phase 3  —  Analyzer    61 / 61   tests passing
Phase 4  —  CodeGen     49 / 49   tests passing
────────────────────────────────────────────────
Total                  259 / 259  ALL PASSING
```

---

## Build

### Prerequisites

Install **MSYS2** (Windows):

```powershell
winget install MSYS2.MSYS2
```

Then open **MSYS2 UCRT64** and install the toolchain:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake --noconfirm
```

Add `C:\msys64\ucrt64\bin` to your system `PATH`.

On **Linux / macOS**:

```bash
# Ubuntu/Debian
sudo apt install g++ cmake

# macOS
brew install cmake  # g++ comes with Xcode Command Line Tools
```

### Compile

```bash
git clone https://github.com/your-username/novalang
cd novalang

mkdir build && cd build
cmake .. -G "MinGW Makefiles"   # Windows (MinGW)
# cmake ..                       # Linux / macOS

make -j4
```

### Run all tests

```bash
./test_lexer
./test_parser
./test_analyzer
./test_codegen
```

---

## Running Nova Programs

Create a `.nova` file:

```nova
// hello.nova
fn greet(name: string) {
    print(name)
}

greet("Nova")
```

> **Note:** A `novac` CLI driver (`src/main.cpp`) is coming in the next phase. For now, programs run through the test harness or a custom `main.cpp` that chains all four phases.

To wire up the pipeline manually in `main.cpp`:

```cpp
#include "src/lexer.h"
#include "src/parser.h"
#include "src/analyzer.h"
#include "src/codegen.h"
#include "src/vm.h"

int main() {
    std::string source = R"(
        fn add(a: int, b: int) -> int { return a + b }
        print(add(3, 5))
    )";

    Lexer    lexer(source);
    Parser   parser(lexer.tokenize());
    auto     prog = parser.parse();

    Analyzer analyzer;
    if (!analyzer.analyze(prog.get())) {
        for (auto& e : analyzer.errors())
            std::cerr << "line " << e.line << ": " << e.message << "\n";
        return 1;
    }

    CodeGen codegen;
    VM      vm(codegen.generate(prog.get()));
    vm.run();
}
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
│   └── codegen.h / codegen.cpp      Phase 4 — AST → Bytecode compiler
│
├── tests/
│   ├── test_lexer.cpp               46  tests
│   ├── test_parser.cpp             103  tests
│   ├── test_analyzer.cpp            61  tests
│   └── test_codegen.cpp             49  tests
│
├── CMakeLists.txt
└── README.md
```

---

## Tech Stack

| Component | Technology |
|---|---|
| Implementation language | C++20 |
| Build system | CMake 3.20+ |
| Compiler (Windows) | MinGW-w64 g++ 15 (MSYS2 UCRT64) |
| Compiler (Linux/macOS) | g++ / clang++ |
| Testing | Hand-written test runner (no external framework) |
| Dependencies | None |

---

## Roadmap

- [ ] `novac` CLI driver — compile `.nova` files from the command line
- [ ] Standard library — `len`, `str`, `int` conversion builtins
- [ ] Arrays and `for` loops
- [ ] User-defined types (`struct`)
- [ ] LLVM backend — native machine code generation
