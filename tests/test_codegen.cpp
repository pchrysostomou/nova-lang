#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/codegen.h"
#include "../src/vm.h"
#include "../src/importer.h"
#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// ── Test framework ─────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

void check(const std::string& name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; passed++; }
    else      { std::cout << "  [FAIL] " << name << "\n"; failed++; }
}

// ── Helper ─────────────────────────────────────────────────────────────────────
//
// Lexes → Parses → Compiles → Runs a Nova program.
// Επιστρέφει την captured output (τα print calls) ή "" αν δεν έχει output.
// Ρίχνει exception αν υπάρξει runtime error.

std::string run(const std::string& src) {
    Lexer    lexer(src);
    Parser   parser(lexer.tokenize());
    auto     prog = parser.parse();

    CodeGen codegen;
    auto    chunks = codegen.generate(prog.get());

    VM vm(std::move(chunks));
    vm.captureOutput = true;
    vm.run();
    return vm.capturedOutput;
}

// ── Literals ───────────────────────────────────────────────────────────────────

void test_print_int() {
    std::cout << "\n[Literals — int]\n";
    check("print 42",   run("print(42)")   == "42\n");
    check("print 0",    run("print(0)")    == "0\n");
    check("print -1",   run("print(-1)")   == "-1\n");
}

void test_print_bool() {
    std::cout << "\n[Literals — bool]\n";
    check("print true",  run("print(true)")  == "true\n");
    check("print false", run("print(false)") == "false\n");
}

void test_print_string() {
    std::cout << "\n[Literals — string]\n";
    check("print hello",  run("print(\"hello\")")  == "hello\n");
    check("print empty",  run("print(\"\")")       == "\n");
}

// ── Arithmetic ─────────────────────────────────────────────────────────────────

void test_arithmetic() {
    std::cout << "\n[Arithmetic]\n";
    check("1 + 2 = 3",    run("print(1 + 2)")    == "3\n");
    check("10 - 3 = 7",   run("print(10 - 3)")   == "7\n");
    check("3 * 4 = 12",   run("print(3 * 4)")    == "12\n");
    check("10 / 2 = 5",   run("print(10 / 2)")   == "5\n");
    check("-5",            run("print(-5)")        == "-5\n");
    check("unary neg",     run("print(-(3))")      == "-3\n");
}

void test_precedence() {
    std::cout << "\n[Precedence]\n";
    // 2 + 3 * 4 = 2 + 12 = 14
    check("2 + 3*4 = 14",      run("print(2 + 3 * 4)")     == "14\n");
    // (2 + 3) * 4 = 20
    check("(2+3)*4 = 20",      run("print((2 + 3) * 4)")   == "20\n");
    // 10 - 3 - 2 = 5 (left-to-right)
    check("10-3-2 = 5",        run("print(10 - 3 - 2)")    == "5\n");
}

// ── Comparisons ────────────────────────────────────────────────────────────────

void test_comparisons() {
    std::cout << "\n[Comparisons]\n";
    check("3 > 2 = true",   run("print(3 > 2)")    == "true\n");
    check("1 > 2 = false",  run("print(1 > 2)")    == "false\n");
    check("1 < 2 = true",   run("print(1 < 2)")    == "true\n");
    check("2 <= 2 = true",  run("print(2 <= 2)")   == "true\n");
    check("3 >= 4 = false", run("print(3 >= 4)")   == "false\n");
    check("1 == 1 = true",  run("print(1 == 1)")   == "true\n");
    check("1 != 2 = true",  run("print(1 != 2)")   == "true\n");
    check("2 == 3 = false", run("print(2 == 3)")   == "false\n");
    check("!true = false",  run("print(!true)")     == "false\n");
    check("!false = true",  run("print(!false)")    == "true\n");
}

// ── Variables ──────────────────────────────────────────────────────────────────

void test_variables() {
    std::cout << "\n[Variables]\n";
    check("let x = 42",
          run("let x = 42\nprint(x)") == "42\n");

    check("let x + let y",
          run("let x = 10\nlet y = x + 5\nprint(y)") == "15\n");

    check("reassign",
          run("let x = 0\nx = 42\nprint(x)") == "42\n");

    check("reassign with expr",
          run("let x = 10\nx = x * 2\nprint(x)") == "20\n");
}

void test_multiple_prints() {
    std::cout << "\n[Multiple statements]\n";
    check("sequence",
          run("print(1)\nprint(2)\nprint(3)") == "1\n2\n3\n");
}

// ── Functions ──────────────────────────────────────────────────────────────────

void test_function_basic() {
    std::cout << "\n[Functions — basic]\n";
    std::string code = R"(
fn add(a: int, b: int) -> int {
    return a + b
}
print(add(3, 5))
)";
    check("add(3,5) = 8", run(code) == "8\n");
}

void test_function_stored_result() {
    std::cout << "\n[Functions — store result in let]\n";
    std::string code = R"(
fn add(a: int, b: int) -> int {
    return a + b
}
let result = add(3, 5)
print(result)
)";
    check("result = 8", run(code) == "8\n");
}

void test_function_no_args() {
    std::cout << "\n[Functions — no args]\n";
    check("constant fn",
          run("fn answer() -> int { return 42 }\nprint(answer())") == "42\n");
}

void test_void_function() {
    std::cout << "\n[Functions — void]\n";
    std::string code = R"(
fn greet() {
    print("hello")
}
greet()
)";
    check("void fn print", run(code) == "hello\n");
}

void test_nested_calls() {
    std::cout << "\n[Functions — nested calls]\n";
    std::string code = R"(
fn double(x: int) -> int {
    return x * 2
}
print(double(double(3)))
)";
    check("double(double(3)) = 12", run(code) == "12\n");
}

void test_function_call_in_expr() {
    std::cout << "\n[Functions — call inside expression]\n";
    std::string code = R"(
fn square(x: int) -> int {
    return x * x
}
print(square(3) + square(4))
)";
    // 9 + 16 = 25
    check("square(3)+square(4) = 25", run(code) == "25\n");
}

// ── Recursion ──────────────────────────────────────────────────────────────────

void test_recursion_factorial() {
    std::cout << "\n[Recursion — factorial]\n";
    std::string code = R"(
fn fact(n: int) -> int {
    if (n == 0) { return 1 }
    else { return n * fact(n - 1) }
}
print(fact(5))
print(fact(0))
print(fact(1))
)";
    check("5! = 120",  run(code) == "120\n1\n1\n");
}

void test_recursion_fibonacci() {
    std::cout << "\n[Recursion — fibonacci]\n";
    std::string code = R"(
fn fib(n: int) -> int {
    if (n <= 1) { return n }
    else { return fib(n - 1) + fib(n - 2) }
}
print(fib(0))
print(fib(1))
print(fib(7))
)";
    // fib(7) = 13
    check("fib results", run(code) == "0\n1\n13\n");
}

// ── If / Else ──────────────────────────────────────────────────────────────────

void test_if_true_branch() {
    std::cout << "\n[If/Else — true branch]\n";
    check("if true",
          run("if (1 < 2) { print(\"yes\") }") == "yes\n");
}

void test_if_false_branch() {
    std::cout << "\n[If/Else — false branch skipped]\n";
    check("if false → no output",
          run("if (2 < 1) { print(\"yes\") }") == "");
}

void test_if_else() {
    std::cout << "\n[If/Else — else branch]\n";
    check("if/else true branch",
          run("if (1 < 2) { print(\"a\") } else { print(\"b\") }") == "a\n");
    check("if/else false branch",
          run("if (2 < 1) { print(\"a\") } else { print(\"b\") }") == "b\n");
}

void test_abs_function() {
    std::cout << "\n[If/Else — abs function]\n";
    std::string code = R"(
fn abs(x: int) -> int {
    if (x >= 0) { return x }
    else { return -x }
}
print(abs(7))
print(abs(-7))
print(abs(0))
)";
    check("abs results", run(code) == "7\n7\n0\n");
}

// ── While loops ────────────────────────────────────────────────────────────────

void test_while_basic() {
    std::cout << "\n[While — basic count]\n";
    std::string code = R"(
let i = 0
while (i < 3) {
    print(i)
    i = i + 1
}
)";
    check("prints 0 1 2", run(code) == "0\n1\n2\n");
}

void test_while_sum() {
    std::cout << "\n[While — sum]\n";
    std::string code = R"(
let i = 0
let sum = 0
while (i < 5) {
    sum = sum + i
    i = i + 1
}
print(sum)
)";
    // 0+1+2+3+4 = 10
    check("sum 0..4 = 10", run(code) == "10\n");
}

void test_while_skipped() {
    std::cout << "\n[While — condition false from start]\n";
    check("no iterations",
          run("let x = 10\nwhile (x < 0) { print(x) }") == "");
}

// ── Global visibility from functions ──────────────────────────────────────────

void test_global_visibility() {
    std::cout << "\n[Scoping — global var visible in function]\n";
    std::string code = R"(
let base = 100
fn addBase(x: int) -> int {
    return x + base
}
print(addBase(5))
)";
    check("5 + 100 = 105", run(code) == "105\n");
}

// ── Modulo operator ───────────────────────────────────────────────────────────

void test_modulo() {
    std::cout << "\n[Modulo — basic]\n";
    check("10 % 3 = 1",  run("print(10 % 3)")  == "1\n");
    check("15 % 5 = 0",  run("print(15 % 5)")  == "0\n");
    check("7 % 2 = 1",   run("print(7 % 2)")   == "1\n");
    check("100 % 7 = 2", run("print(100 % 7)") == "2\n");
}

void test_modulo_in_function() {
    std::cout << "\n[Modulo — in function / fizzbuzz]\n";
    std::string code = R"(
fn isEven(n: int) -> bool {
    return n % 2 == 0
}
print(isEven(4))
print(isEven(7))
)";
    check("4 is even",   run(code) == "true\nfalse\n");

    std::string fizz = R"(
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
fizzbuzz(15)
)";
    std::string expected =
        "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n11\nFizz\n13\n14\nFizzBuzz\n";
    check("fizzbuzz(15)", run(fizz) == expected);
}

// ── String concatenation ──────────────────────────────────────────────────────

void test_string_concat() {
    std::cout << "\n[String — concatenation]\n";
    check("hello + world",
          run(R"(print("hello" + " world"))") == "hello world\n");
    check("three parts",
          run(R"(print("foo" + "bar" + "baz"))") == "foobarbaz\n");
    check("empty + str",
          run(R"(print("" + "abc"))") == "abc\n");
    check("str + empty",
          run(R"(print("abc" + ""))") == "abc\n");
}

void test_string_concat_in_function() {
    std::cout << "\n[String — concat in function]\n";
    std::string code = R"(
fn greet(name: string) -> string {
    return "Hello, " + name + "!"
}
print(greet("Nova"))
print(greet("World"))
)";
    check("greet Nova",  run(code) == "Hello, Nova!\nHello, World!\n");

    std::string code2 = R"(
fn join(a: string, b: string) -> string {
    return a + " " + b
}
let result = join("foo", "bar")
print(result)
)";
    check("join foo bar", run(code2) == "foo bar\n");
}

// ── For loops ─────────────────────────────────────────────────────────────────

void test_for_basic() {
    std::cout << "\n[For — basic count]\n";
    check("0..4",
          run("for (let i = 0; i < 5; i = i + 1) { print(i) }")
          == "0\n1\n2\n3\n4\n");
}

void test_for_zero_iterations() {
    std::cout << "\n[For — condition false from start]\n";
    check("no iters",
          run("for (let i = 0; i < 0; i = i + 1) { print(i) }") == "");
}

void test_for_sum() {
    std::cout << "\n[For — accumulate sum]\n";
    std::string code = R"(
let sum = 0
for (let i = 1; i <= 10; i = i + 1) {
    sum = sum + i
}
print(sum)
)";
    check("sum 1..10 = 55", run(code) == "55\n");
}

void test_for_in_function() {
    std::cout << "\n[For — inside function]\n";
    std::string code = R"(
fn sumTo(n: int) -> int {
    let s = 0
    for (let i = 1; i <= n; i = i + 1) {
        s = s + i
    }
    return s
}
print(sumTo(5))
print(sumTo(100))
)";
    check("sumTo(5)=15, sumTo(100)=5050", run(code) == "15\n5050\n");
}

void test_for_nested() {
    std::cout << "\n[For — nested loops]\n";
    std::string code = R"(
let count = 0
for (let i = 0; i < 3; i = i + 1) {
    for (let j = 0; j < 3; j = j + 1) {
        count = count + 1
    }
}
print(count)
)";
    check("3x3 = 9", run(code) == "9\n");
}

// ── else if ───────────────────────────────────────────────────────────────────

void test_else_if() {
    std::cout << "\n[Else if — basic chain]\n";

    std::string grade = R"(
fn grade(n: int) -> string {
    if (n >= 90) { return "A" }
    else if (n >= 80) { return "B" }
    else if (n >= 70) { return "C" }
    else if (n >= 60) { return "D" }
    else { return "F" }
}
print(grade(95))
print(grade(85))
print(grade(75))
print(grade(65))
print(grade(55))
)";
    check("grade chain", run(grade) == "A\nB\nC\nD\nF\n");

    std::string sign = R"(
fn sign(n: int) -> string {
    if (n > 0) { return "positive" }
    else if (n < 0) { return "negative" }
    else { return "zero" }
}
print(sign(5))
print(sign(-3))
print(sign(0))
)";
    check("sign(5)",  run(sign) == "positive\nnegative\nzero\n");

    // else if with no final else — only matching branch prints
    std::string noelse = R"(
let x = 2
if (x == 1) { print("one") }
else if (x == 2) { print("two") }
else if (x == 3) { print("three") }
)";
    check("else if no final else", run(noelse) == "two\n");
}

// ── Arrays ────────────────────────────────────────────────────────────────────

void test_array_literal() {
    std::cout << "\n[Array — literal and indexing]\n";
    check("arr[0]", run("let a = [10, 20, 30]\nprint(a[0])") == "10\n");
    check("arr[1]", run("let a = [10, 20, 30]\nprint(a[1])") == "20\n");
    check("arr[2]", run("let a = [10, 20, 30]\nprint(a[2])") == "30\n");
}

void test_array_set() {
    std::cout << "\n[Array — element assignment]\n";
    check("set arr[1]",
          run("let a = [1, 2, 3]\na[1] = 99\nprint(a[1])") == "99\n");
    check("set then read others",
          run("let a = [1, 2, 3]\na[0] = 42\nprint(a[0])\nprint(a[2])") == "42\n3\n");
}

void test_array_print() {
    std::cout << "\n[Array — print whole array]\n";
    check("print [1,2,3]",
          run("let a = [1, 2, 3]\nprint(a)") == "[1, 2, 3]\n");
    check("print empty",
          run("let a = []\nprint(a)") == "[]\n");
}

void test_array_in_loop() {
    std::cout << "\n[Array — iterate with for]\n";
    std::string code = R"(
let a = [10, 20, 30, 40, 50]
let sum = 0
for (let i = 0; i < 5; i = i + 1) {
    sum = sum + a[i]
}
print(sum)
)";
    check("sum of elements", run(code) == "150\n");
}

void test_array_in_function() {
    std::cout << "\n[Array — passed to function]\n";
    std::string code = R"(
fn first(a: int) -> int {
    return a
}
let arr = [7, 8, 9]
print(first(arr[0]))
print(first(arr[2]))
)";
    check("arr[0] and arr[2] to fn", run(code) == "7\n9\n");
}

// ── Import helper ─────────────────────────────────────────────────────────────
//
// Writes a set of files to a unique temp directory, resolves imports in
// `mainFile`, compiles and runs it, returns captured output.
// The temp directory is deleted afterwards.

static std::string runImport(
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::string& mainFile)
{
    fs::path dir = fs::temp_directory_path() /
                   ("nova_test_" + std::to_string(std::rand()));
    fs::create_directories(dir);

    for (const auto& [name, src] : files)
        std::ofstream(dir / name) << src;

    std::string src;
    {
        std::ifstream f(dir / mainFile);
        src.assign((std::istreambuf_iterator<char>(f)), {});
    }  // f closed here

    Lexer  lexer(src);
    Parser parser(lexer.tokenize());
    auto   prog   = parser.parse();
    auto   merged = resolveImports(std::move(prog), dir.string());

    CodeGen codegen;
    auto    chunks = codegen.generate(merged.get());

    VM vm(std::move(chunks));
    vm.captureOutput = true;
    vm.run();

    fs::remove_all(dir);
    return vm.capturedOutput;
}

// ── Import tests ──────────────────────────────────────────────────────────────

void test_import_basic() {
    std::cout << "\n[Import — basic function from lib]\n";

    std::string lib = R"(
fn square(x: int) -> int {
    return x * x
}
fn cube(x: int) -> int {
    return x * x * x
}
)";
    std::string main = R"(
import "lib.nova"
print(square(4))
print(cube(3))
)";
    check("square(4)=16, cube(3)=27",
          runImport({{"lib.nova", lib}, {"main.nova", main}}, "main.nova")
          == "16\n27\n");
}

void test_import_multiple() {
    std::cout << "\n[Import — multiple libs]\n";

    std::string mathLib = R"(
fn double(x: int) -> int { return x * 2 }
)";
    std::string strLib = R"(
fn greet(name: string) -> string { return "Hello, " + name }
)";
    std::string main = R"(
import "math.nova"
import "strings.nova"
print(double(21))
print(greet("Nova"))
)";
    check("double + greet",
          runImport({{"math.nova", mathLib}, {"strings.nova", strLib}, {"main.nova", main}},
                    "main.nova")
          == "42\nHello, Nova\n");
}

void test_import_transitive() {
    std::cout << "\n[Import — transitive (A imports B imports C)]\n";

    std::string c = R"(
fn base(x: int) -> int { return x + 1 }
)";
    std::string b = R"(
import "c.nova"
fn mid(x: int) -> int { return base(x) * 2 }
)";
    std::string a = R"(
import "b.nova"
print(mid(4))
)";
    // mid(4) = base(4)*2 = 5*2 = 10
    check("transitive: mid(4)=10",
          runImport({{"c.nova", c}, {"b.nova", b}, {"a.nova", a}}, "a.nova")
          == "10\n");
}

void test_import_dedup() {
    std::cout << "\n[Import — deduplication (shared dependency)]\n";

    std::string shared = R"(
fn add(a: int, b: int) -> int { return a + b }
)";
    std::string lib1 = R"(
import "shared.nova"
fn triple(x: int) -> int { return add(x, add(x, x)) }
)";
    std::string lib2 = R"(
import "shared.nova"
fn quadruple(x: int) -> int { return add(add(x, x), add(x, x)) }
)";
    std::string main = R"(
import "lib1.nova"
import "lib2.nova"
print(triple(3))
print(quadruple(3))
)";
    // shared.nova inlined only once; both functions work
    check("dedup: triple=9, quad=12",
          runImport({{"shared.nova", shared}, {"lib1.nova", lib1},
                     {"lib2.nova", lib2}, {"main.nova", main}},
                    "main.nova")
          == "9\n12\n");
}

void test_import_missing_file() {
    std::cout << "\n[Import — missing file throws]\n";

    fs::path dir = fs::temp_directory_path() /
                   ("nova_test_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    std::ofstream(dir / "main.nova") << "import \"missing.nova\"\n";

    std::string src;
    { std::ifstream f(dir / "main.nova"); src.assign((std::istreambuf_iterator<char>(f)), {}); }

    Lexer  lexer(src);
    Parser parser(lexer.tokenize());
    auto   prog = parser.parse();

    bool threw = false;
    try {
        resolveImports(std::move(prog), dir.string());
    } catch (const std::exception&) {
        threw = true;
    }
    fs::remove_all(dir);
    check("missing file throws", threw);
}

void test_import_circular() {
    std::cout << "\n[Import — circular import throws]\n";

    std::string a = "import \"b.nova\"\n";
    std::string b = "import \"a.nova\"\n";

    fs::path dir = fs::temp_directory_path() /
                   ("nova_test_" + std::to_string(std::rand()));
    fs::create_directories(dir);
    std::ofstream(dir / "a.nova") << a;
    std::ofstream(dir / "b.nova") << b;

    std::string src;
    { std::ifstream f(dir / "a.nova"); src.assign((std::istreambuf_iterator<char>(f)), {}); }

    Lexer  lexer(src);
    Parser parser(lexer.tokenize());
    auto   prog = parser.parse();

    bool threw = false;
    try {
        resolveImports(std::move(prog), dir.string());
    } catch (const std::exception&) {
        threw = true;
    }
    fs::remove_all(dir);
    check("circular import throws", threw);
}

// ── Full blueprint program ─────────────────────────────────────────────────────

void test_full_blueprint() {
    std::cout << "\n[Full blueprint program]\n";
    std::string code = R"(
fn add(a: int, b: int) -> int {
    return a + b
}
let result = add(3, 5)
print(result)
)";
    check("add(3,5) = 8", run(code) == "8\n");
}

// ── Entry point ────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Nova Bytecode VM Tests ===\n";

    test_print_int();
    test_print_bool();
    test_print_string();
    test_arithmetic();
    test_precedence();
    test_comparisons();
    test_variables();
    test_multiple_prints();
    test_function_basic();
    test_function_stored_result();
    test_function_no_args();
    test_void_function();
    test_nested_calls();
    test_function_call_in_expr();
    test_recursion_factorial();
    test_recursion_fibonacci();
    test_if_true_branch();
    test_if_false_branch();
    test_if_else();
    test_abs_function();
    test_while_basic();
    test_while_sum();
    test_while_skipped();
    test_global_visibility();
    test_modulo();
    test_modulo_in_function();
    test_string_concat();
    test_string_concat_in_function();
    test_for_basic();
    test_for_zero_iterations();
    test_for_sum();
    test_for_in_function();
    test_for_nested();
    test_else_if();
    test_array_literal();
    test_array_set();
    test_array_print();
    test_array_in_loop();
    test_array_in_function();
    test_import_basic();
    test_import_multiple();
    test_import_transitive();
    test_import_dedup();
    test_import_missing_file();
    test_import_circular();
    test_full_blueprint();

    std::cout << "\n═══════════════════════════\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";

    return failed == 0 ? 0 : 1;
}
