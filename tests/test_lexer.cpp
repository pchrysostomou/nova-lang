#include "../src/lexer.h"
#include <iostream>
#include <string>

// ── Μικρό test framework ───────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

void check(const std::string& testName, bool condition) {
    if (condition) {
        std::cout << "  [PASS] " << testName << "\n";
        passed++;
    } else {
        std::cout << "  [FAIL] " << testName << "\n";
        failed++;
    }
}

// Βοηθητική: παίρνει source, επιστρέφει tokens χωρίς EOF
std::vector<Token> lex(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    if (!tokens.empty() && tokens.back().type == TokenType::EOF_TOKEN)
        tokens.pop_back();
    return tokens;
}

// ── Tests ──────────────────────────────────────────────────────────────────────

void test_numbers() {
    std::cout << "\n[Numbers]\n";
    auto t = lex("42");
    check("single integer", t.size() == 1 && t[0].type == TokenType::NUMBER && t[0].value == "42");

    t = lex("3.14");
    check("float", t.size() == 1 && t[0].type == TokenType::NUMBER && t[0].value == "3.14");

    t = lex("0");
    check("zero", t.size() == 1 && t[0].value == "0");
}

void test_strings() {
    std::cout << "\n[Strings]\n";
    auto t = lex("\"hello\"");
    check("simple string", t.size() == 1 && t[0].type == TokenType::STRING && t[0].value == "hello");

    t = lex("\"\"");
    check("empty string", t.size() == 1 && t[0].type == TokenType::STRING && t[0].value == "");
}

void test_keywords() {
    std::cout << "\n[Keywords]\n";
    auto t = lex("let fn return if else while true false");
    check("let",    t[0].type == TokenType::LET);
    check("fn",     t[1].type == TokenType::FN);
    check("return", t[2].type == TokenType::RETURN);
    check("if",     t[3].type == TokenType::IF);
    check("else",   t[4].type == TokenType::ELSE);
    check("while",  t[5].type == TokenType::WHILE);
    check("true",   t[6].type == TokenType::TRUE_KW);
    check("false",  t[7].type == TokenType::FALSE_KW);
}

void test_identifiers() {
    std::cout << "\n[Identifiers]\n";
    auto t = lex("x myVar _private foo123");
    check("x",        t[0].type == TokenType::IDENTIFIER && t[0].value == "x");
    check("myVar",    t[1].type == TokenType::IDENTIFIER && t[1].value == "myVar");
    check("_private", t[2].type == TokenType::IDENTIFIER && t[2].value == "_private");
    check("foo123",   t[3].type == TokenType::IDENTIFIER && t[3].value == "foo123");
}

void test_operators() {
    std::cout << "\n[Operators]\n";
    auto t = lex("+ - * / = == ! != < > <= >=");
    check("+",  t[0].type == TokenType::PLUS);
    check("-",  t[1].type == TokenType::MINUS);
    check("*",  t[2].type == TokenType::STAR);
    check("/",  t[3].type == TokenType::SLASH);
    check("=",  t[4].type == TokenType::EQUALS);
    check("==", t[5].type == TokenType::EQEQ);
    check("!",  t[6].type == TokenType::BANG);
    check("!=", t[7].type == TokenType::NEQ);
    check("<",  t[8].type == TokenType::LT);
    check(">",  t[9].type == TokenType::GT);
    check("<=", t[10].type == TokenType::LEQ);
    check(">=", t[11].type == TokenType::GEQ);
}

void test_symbols() {
    std::cout << "\n[Symbols]\n";
    auto t = lex("( ) { } : -> , ;");
    check("(",  t[0].type == TokenType::LPAREN);
    check(")",  t[1].type == TokenType::RPAREN);
    check("{",  t[2].type == TokenType::LBRACE);
    check("}",  t[3].type == TokenType::RBRACE);
    check(":",  t[4].type == TokenType::COLON);
    check("->", t[5].type == TokenType::ARROW);
    check(",",  t[6].type == TokenType::COMMA);
    check(";",  t[7].type == TokenType::SEMICOLON);
}

void test_line_tracking() {
    std::cout << "\n[Line Tracking]\n";
    auto t = lex("let\nx");
    check("let is line 1", t[0].line == 1);
    check("x is line 2",   t[1].line == 2);
}

void test_comments() {
    std::cout << "\n[Comments]\n";
    auto t = lex("let x // this is a comment\nlet y");
    check("ignores comment", t.size() == 4);
    check("let after comment", t[2].type == TokenType::LET);
}

void test_real_nova_code() {
    std::cout << "\n[Real Nova Code]\n";
    std::string code = R"(
fn add(a: int, b: int) -> int {
    return a + b
}

let result = add(3, 5)
)";
    auto t = lex(code);

    check("starts with fn",      t[0].type == TokenType::FN);
    check("function name 'add'", t[1].type == TokenType::IDENTIFIER && t[1].value == "add");
    check("has arrow ->",        t[11].type == TokenType::ARROW);
    check("has return keyword",  t[14].type == TokenType::RETURN);
    check("has let",             t[19].type == TokenType::LET);
}

// ── Entry point ────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Nova Lexer Tests ===\n";

    test_numbers();
    test_strings();
    test_keywords();
    test_identifiers();
    test_operators();
    test_symbols();
    test_line_tracking();
    test_comments();
    test_real_nova_code();

    std::cout << "\n═══════════════════════\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";

    return failed == 0 ? 0 : 1;
}
