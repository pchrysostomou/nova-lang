#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/analyzer.h"
#include <iostream>
#include <string>
#include <vector>

// ── Test framework ─────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

void check(const std::string& name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; passed++; }
    else      { std::cout << "  [FAIL] " << name << "\n"; failed++; }
}

// ── Helper ─────────────────────────────────────────────────────────────────────

struct Result {
    std::vector<Diagnostic> errors;
    bool ok() const { return errors.empty(); }
    int  count() const { return (int)errors.size(); }

    // Ελέγχει αν κάποιο μήνυμα περιέχει το substring
    bool hasMsg(const std::string& sub) const {
        for (const auto& e : errors)
            if (e.message.find(sub) != std::string::npos) return true;
        return false;
    }
};

Result analyze(const std::string& src) {
    Lexer    lexer(src);
    Parser   parser(lexer.tokenize());
    auto     prog = parser.parse();
    Analyzer analyzer;
    analyzer.analyze(prog.get());
    return {analyzer.errors()};
}

// ── Έγκυρα προγράμματα (0 σφάλματα) ──────────────────────────────────────────

void test_valid_let() {
    std::cout << "\n[Valid — let declarations]\n";
    auto r = analyze("let x = 42\nlet y = x + 1");
    check("no errors",       r.ok());
    check("count == 0",      r.count() == 0);
}

void test_valid_function() {
    std::cout << "\n[Valid — function with return]\n";
    auto r = analyze("fn add(a: int, b: int) -> int { return a + b }");
    check("no errors", r.ok());
}

void test_valid_void_function() {
    std::cout << "\n[Valid — void function (no return needed)]\n";
    auto r = analyze("fn greet() { let msg = \"hello\" }");
    check("no errors", r.ok());
}

void test_valid_if_else_both_return() {
    std::cout << "\n[Valid — if/else both branches return]\n";
    auto r = analyze(R"(
fn abs(x: int) -> int {
    if (x >= 0) { return x }
    else { return -x }
}
)");
    check("no errors", r.ok());
}

void test_valid_global_var_in_function() {
    std::cout << "\n[Valid — function reads global variable]\n";
    auto r = analyze("let x = 10\nfn f() -> int { return x }");
    check("no errors", r.ok());
}

void test_valid_params_in_body() {
    std::cout << "\n[Valid — params visible inside function body]\n";
    auto r = analyze("fn double(x: int) -> int { return x * 2 }");
    check("no errors", r.ok());
}

void test_valid_recursive_function() {
    std::cout << "\n[Valid — recursive function]\n";
    auto r = analyze(R"(
fn fact(n: int) -> int {
    if (n == 0) { return 1 }
    else { return n * fact(n - 1) }
}
)");
    check("no errors", r.ok());
}

void test_valid_print_builtin() {
    std::cout << "\n[Valid — print builtin]\n";
    auto r = analyze("let x = 42\nprint(x)");
    check("no errors", r.ok());
}

void test_valid_let_with_type() {
    std::cout << "\n[Valid — let with matching explicit type]\n";
    auto r = analyze("let x: int = 42");
    check("no errors", r.ok());
}

void test_valid_return_after_if() {
    std::cout << "\n[Valid — return after if (no else needed)]\n";
    // if without else, then a return after → still guaranteed return
    auto r = analyze(R"(
fn f(x: int) -> int {
    if (x > 0) { return x }
    return 0
}
)");
    check("no errors", r.ok());
}

void test_valid_while_loop() {
    std::cout << "\n[Valid — while loop]\n";
    auto r = analyze(R"(
fn count() {
    let i = 0
    while (i < 10) { i = i + 1 }
}
)");
    check("no errors", r.ok());
}

void test_valid_mutual_calls() {
    std::cout << "\n[Valid — functions defined after their call site]\n";
    // main calls add, but add is defined below — works because of 2-pass collection
    auto r = analyze(R"(
fn main() {
    let r = add(1, 2)
}
fn add(a: int, b: int) -> int { return a + b }
)");
    check("no errors", r.ok());
}

// ── Undefined variable ─────────────────────────────────────────────────────────

void test_undefined_variable() {
    std::cout << "\n[Error — undefined variable]\n";
    auto r = analyze("let y = x + 1");
    check("has 1 error",      r.count() == 1);
    check("mentions 'x'",     r.hasMsg("'x'"));
    check("says 'undefined'", r.hasMsg("undefined"));
}

void test_undefined_in_function() {
    std::cout << "\n[Error — undefined variable inside function]\n";
    auto r = analyze("fn f() { let y = z }");
    check("has error",        r.count() >= 1);
    check("mentions 'z'",     r.hasMsg("'z'"));
}

// ── Duplicate declarations ─────────────────────────────────────────────────────

void test_duplicate_variable() {
    std::cout << "\n[Error — duplicate variable]\n";
    auto r = analyze("let x = 1\nlet x = 2");
    check("has 1 error",       r.count() == 1);
    check("mentions 'x'",      r.hasMsg("'x'"));
    check("says 'already'",    r.hasMsg("already"));
}

void test_duplicate_function() {
    std::cout << "\n[Error — duplicate function]\n";
    auto r = analyze("fn f() { }\nfn f() { }");
    check("has 1 error",         r.count() == 1);
    check("mentions 'f'",        r.hasMsg("'f'"));
    check("says 'already'",      r.hasMsg("already"));
}

void test_shadowing_allowed() {
    std::cout << "\n[Valid — shadowing in inner scope is OK]\n";
    // x in outer scope, then x re-declared inside function — allowed
    auto r = analyze("let x = 1\nfn f() { let x = 2 }");
    check("no errors (shadowing allowed)", r.ok());
}

// ── Type mismatches ────────────────────────────────────────────────────────────

void test_type_mismatch_decl() {
    std::cout << "\n[Error — type mismatch in declaration]\n";
    auto r = analyze("let x: int = \"hello\"");
    check("has 1 error",         r.count() == 1);
    check("mentions 'int'",      r.hasMsg("int"));
    check("mentions 'string'",   r.hasMsg("string"));
}

void test_type_mismatch_binary() {
    std::cout << "\n[Error — type mismatch in binary op]\n";
    auto r = analyze("let x = 1 + \"hello\"");
    check("has error(s)",        r.count() >= 1);
    check("mentions 'string'",   r.hasMsg("string"));
}

void test_type_mismatch_return() {
    std::cout << "\n[Error — return type mismatch]\n";
    auto r = analyze("fn f() -> int { return \"oops\" }");
    check("has 1 error",         r.count() == 1);
    check("mentions 'int'",      r.hasMsg("int"));
    check("mentions 'string'",   r.hasMsg("string"));
}

void test_type_mismatch_assignment() {
    std::cout << "\n[Error — assignment type mismatch]\n";
    auto r = analyze("let x: int = 5\nx = \"hello\"");
    check("has 1 error",         r.count() == 1);
    check("mentions 'int'",      r.hasMsg("int"));
    check("mentions 'string'",   r.hasMsg("string"));
}

void test_type_mismatch_comparison() {
    std::cout << "\n[Error — comparing incompatible types]\n";
    auto r = analyze("let x = 1 == \"hello\"");
    check("has error",           r.count() >= 1);
}

void test_unary_type_mismatch() {
    std::cout << "\n[Error — unary '-' on string]\n";
    auto r = analyze("let x = -\"hello\"");
    check("has error",           r.count() >= 1);
    check("mentions 'string'",   r.hasMsg("string"));
}

void test_unary_not_on_int() {
    std::cout << "\n[Error — '!' on int]\n";
    auto r = analyze("let x = !42");
    check("has error",           r.count() >= 1);
    check("mentions 'bool'",     r.hasMsg("bool"));
}

// ── Missing return ─────────────────────────────────────────────────────────────

void test_missing_return_no_return() {
    std::cout << "\n[Error — no return at all]\n";
    auto r = analyze("fn add(a: int, b: int) -> int { let x = 1 }");
    check("has 1 error",         r.count() == 1);
    check("mentions 'add'",      r.hasMsg("add"));
    check("says 'missing'",      r.hasMsg("missing"));
}

void test_missing_return_if_no_else() {
    std::cout << "\n[Error — if without else doesn't guarantee return]\n";
    auto r = analyze(R"(
fn f(x: int) -> int {
    if (x > 0) { return x }
}
)");
    check("has 1 error",         r.count() == 1);
    check("says 'missing'",      r.hasMsg("missing"));
}

void test_bare_return_in_non_void() {
    std::cout << "\n[Error — bare return in non-void function]\n";
    auto r = analyze("fn f() -> int { return }");
    // bare return → 1 error; AND the bare return doesn't count as "returns",
    // so missing-return is also triggered → 2 errors
    check("has error(s)",        r.count() >= 1);
    check("mentions non-void",   r.hasMsg("non-void") || r.hasMsg("int"));
}

// ── Undefined function ─────────────────────────────────────────────────────────

void test_undefined_function() {
    std::cout << "\n[Error — call to undefined function]\n";
    auto r = analyze("foo()");
    check("has 1 error",         r.count() == 1);
    check("mentions 'foo'",      r.hasMsg("'foo'"));
    check("says 'undefined'",    r.hasMsg("undefined"));
}

// ── Wrong argument count / types ───────────────────────────────────────────────

void test_wrong_arg_count_too_few() {
    std::cout << "\n[Error — too few arguments]\n";
    auto r = analyze("fn add(a: int, b: int) -> int { return a + b }\nadd(1)");
    check("has 1 error",         r.count() == 1);
    check("mentions 'add'",      r.hasMsg("add"));
}

void test_wrong_arg_count_too_many() {
    std::cout << "\n[Error — too many arguments]\n";
    auto r = analyze("fn f(x: int) -> int { return x }\nf(1, 2, 3)");
    check("has 1 error",         r.count() == 1);
}

void test_wrong_arg_type() {
    std::cout << "\n[Error — wrong argument type]\n";
    auto r = analyze("fn add(a: int, b: int) -> int { return a + b }\nadd(1, \"two\")");
    check("has 1 error",         r.count() == 1);
    check("mentions 'int'",      r.hasMsg("int"));
    check("mentions 'string'",   r.hasMsg("string"));
}

// ── Variable scope ─────────────────────────────────────────────────────────────

void test_var_not_visible_outside_block() {
    std::cout << "\n[Error — variable declared in if block, used outside]\n";
    auto r = analyze(R"(
fn f() {
    if (true) { let inner = 1 }
    let y = inner
}
)");
    check("has error",           r.count() >= 1);
    check("mentions 'inner'",    r.hasMsg("inner"));
}

void test_var_not_visible_outside_function() {
    std::cout << "\n[Error — local var not visible in another function]\n";
    auto r = analyze(R"(
fn a() { let x = 1 }
fn b() -> int { return x }
)");
    check("has error",           r.count() >= 1);
    check("mentions 'x'",        r.hasMsg("'x'"));
}

// ── Entry point ────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Nova Semantic Analyzer Tests ===\n";

    // Έγκυρα προγράμματα
    test_valid_let();
    test_valid_function();
    test_valid_void_function();
    test_valid_if_else_both_return();
    test_valid_global_var_in_function();
    test_valid_params_in_body();
    test_valid_recursive_function();
    test_valid_print_builtin();
    test_valid_let_with_type();
    test_valid_return_after_if();
    test_valid_while_loop();
    test_valid_mutual_calls();

    // Undefined variable
    test_undefined_variable();
    test_undefined_in_function();

    // Duplicate declarations
    test_duplicate_variable();
    test_duplicate_function();
    test_shadowing_allowed();

    // Type mismatches
    test_type_mismatch_decl();
    test_type_mismatch_binary();
    test_type_mismatch_return();
    test_type_mismatch_assignment();
    test_type_mismatch_comparison();
    test_unary_type_mismatch();
    test_unary_not_on_int();

    // Missing return
    test_missing_return_no_return();
    test_missing_return_if_no_else();
    test_bare_return_in_non_void();

    // Undefined function
    test_undefined_function();

    // Wrong arg count / types
    test_wrong_arg_count_too_few();
    test_wrong_arg_count_too_many();
    test_wrong_arg_type();

    // Scope isolation
    test_var_not_visible_outside_block();
    test_var_not_visible_outside_function();

    std::cout << "\n═══════════════════════════════════\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";

    return failed == 0 ? 0 : 1;
}
