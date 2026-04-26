#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/ast.h"
#include <iostream>
#include <string>

// ── Test framework ─────────────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

void check(const std::string& name, bool cond) {
    if (cond) { std::cout << "  [PASS] " << name << "\n"; passed++; }
    else      { std::cout << "  [FAIL] " << name << "\n"; failed++; }
}

// ── Helpers ────────────────────────────────────────────────────────────────────

std::unique_ptr<Program> parse(const std::string& src) {
    Lexer  lexer(src);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

// Cast με null-check — επιστρέφει nullptr αν αποτύχει
template<typename T>
T* as(ASTNode* node) { return dynamic_cast<T*>(node); }

// Παίρνει το statement[i] από ένα Program
ASTNode* stmt(const std::unique_ptr<Program>& p, size_t i) {
    return (i < p->statements.size()) ? p->statements[i].get() : nullptr;
}

// ── Δοκιμές: VarDecl ───────────────────────────────────────────────────────────

void test_var_decl_number() {
    std::cout << "\n[VarDecl — αριθμός]\n";
    auto prog = parse("let x = 42");

    auto* decl = as<VarDecl>(stmt(prog, 0));
    check("is VarDecl",             decl != nullptr);
    check("name is 'x'",            decl && decl->name == "x");
    check("type is empty",          decl && decl->type.empty());

    auto* num = decl ? as<NumberLiteral>(decl->value.get()) : nullptr;
    check("value is NumberLiteral", num != nullptr);
    check("value == 42",            num && num->value == 42.0);
}

void test_var_decl_with_type() {
    std::cout << "\n[VarDecl — με τύπο]\n";
    auto prog = parse("let x: int = 42");

    auto* decl = as<VarDecl>(stmt(prog, 0));
    check("has type 'int'", decl && decl->type == "int");
    check("name is 'x'",    decl && decl->name == "x");
}

void test_var_decl_string() {
    std::cout << "\n[VarDecl — string]\n";
    auto prog = parse("let msg = \"hello\"");

    auto* decl = as<VarDecl>(stmt(prog, 0));
    auto* str  = decl ? as<StringLiteral>(decl->value.get()) : nullptr;
    check("value is StringLiteral", str != nullptr);
    check("value == 'hello'",       str && str->value == "hello");
}

void test_var_decl_bool() {
    std::cout << "\n[VarDecl — bool]\n";
    auto prog = parse("let a = true\nlet b = false");

    auto* d1 = as<VarDecl>(stmt(prog, 0));
    auto* d2 = as<VarDecl>(stmt(prog, 1));

    auto* t = d1 ? as<BoolLiteral>(d1->value.get()) : nullptr;
    auto* f = d2 ? as<BoolLiteral>(d2->value.get()) : nullptr;

    check("true is BoolLiteral",   t != nullptr);
    check("true value == true",    t && t->value == true);
    check("false value == false",  f && f->value == false);
}

// ── Δοκιμές: FunctionDecl ─────────────────────────────────────────────────────

void test_function_decl() {
    std::cout << "\n[FunctionDecl]\n";
    auto prog = parse("fn add(a: int, b: int) -> int { return a + b }");

    auto* fn = as<FunctionDecl>(stmt(prog, 0));
    check("is FunctionDecl",      fn != nullptr);
    check("name == 'add'",        fn && fn->name == "add");
    check("2 params",             fn && fn->params.size() == 2);
    check("param[0].name == 'a'", fn && fn->params[0].name == "a");
    check("param[0].type == 'int'", fn && fn->params[0].type == "int");
    check("param[1].name == 'b'", fn && fn->params[1].name == "b");
    check("param[1].type == 'int'", fn && fn->params[1].type == "int");
    check("returnType == 'int'",  fn && fn->returnType == "int");
    check("body has 1 stmt",      fn && fn->body.size() == 1);

    auto* ret = fn ? as<ReturnStmt>(fn->body[0].get()) : nullptr;
    check("body[0] is ReturnStmt", ret != nullptr);

    auto* binop = ret ? as<BinaryOp>(ret->value.get()) : nullptr;
    check("return value is BinaryOp", binop != nullptr);
    check("op == '+'",                binop && binop->op == "+");

    auto* lhs = binop ? as<Identifier>(binop->left.get()) : nullptr;
    auto* rhs = binop ? as<Identifier>(binop->right.get()) : nullptr;
    check("left == 'a'",  lhs && lhs->name == "a");
    check("right == 'b'", rhs && rhs->name == "b");
}

void test_function_no_return_type() {
    std::cout << "\n[FunctionDecl — χωρίς return type]\n";
    auto prog = parse("fn greet() { }");

    auto* fn = as<FunctionDecl>(stmt(prog, 0));
    check("is FunctionDecl",       fn != nullptr);
    check("0 params",              fn && fn->params.empty());
    check("returnType is empty",   fn && fn->returnType.empty());
    check("body is empty",         fn && fn->body.empty());
}

// ── Δοκιμές: ReturnStmt ───────────────────────────────────────────────────────

void test_return_with_value() {
    std::cout << "\n[ReturnStmt — με τιμή]\n";
    auto prog = parse("fn f() { return 0 }");
    auto* fn  = as<FunctionDecl>(stmt(prog, 0));
    auto* ret = fn ? as<ReturnStmt>(fn->body[0].get()) : nullptr;

    check("is ReturnStmt",     ret != nullptr);
    check("has value",         ret && ret->value != nullptr);
    auto* num = ret ? as<NumberLiteral>(ret->value.get()) : nullptr;
    check("value == 0",        num && num->value == 0.0);
}

void test_return_no_value() {
    std::cout << "\n[ReturnStmt — bare return]\n";
    auto prog = parse("fn f() { return }");
    auto* fn  = as<FunctionDecl>(stmt(prog, 0));
    auto* ret = fn ? as<ReturnStmt>(fn->body[0].get()) : nullptr;

    check("is ReturnStmt",      ret != nullptr);
    check("value is null",      ret && ret->value == nullptr);
}

// ── Δοκιμές: FunctionCall ─────────────────────────────────────────────────────

void test_function_call_with_args() {
    std::cout << "\n[FunctionCall — με args]\n";
    auto prog = parse("add(3, 5)");

    auto* es   = as<ExprStmt>(stmt(prog, 0));
    auto* call = es ? as<FunctionCall>(es->expr.get()) : nullptr;
    check("is FunctionCall",  call != nullptr);
    check("name == 'add'",    call && call->name == "add");
    check("2 args",           call && call->args.size() == 2);

    auto* a0 = call ? as<NumberLiteral>(call->args[0].get()) : nullptr;
    auto* a1 = call ? as<NumberLiteral>(call->args[1].get()) : nullptr;
    check("arg[0] == 3",  a0 && a0->value == 3.0);
    check("arg[1] == 5",  a1 && a1->value == 5.0);
}

void test_function_call_no_args() {
    std::cout << "\n[FunctionCall — χωρίς args]\n";
    auto prog = parse("greet()");

    auto* es   = as<ExprStmt>(stmt(prog, 0));
    auto* call = es ? as<FunctionCall>(es->expr.get()) : nullptr;
    check("is FunctionCall", call != nullptr);
    check("0 args",          call && call->args.empty());
}

void test_nested_call() {
    std::cout << "\n[FunctionCall — nested]\n";
    // print(add(1, 2))
    auto prog = parse("print(add(1, 2))");

    auto* es    = as<ExprStmt>(stmt(prog, 0));
    auto* outer = es ? as<FunctionCall>(es->expr.get()) : nullptr;
    check("outer is FunctionCall",    outer != nullptr);
    check("outer name == 'print'",    outer && outer->name == "print");
    check("outer has 1 arg",          outer && outer->args.size() == 1);

    auto* inner = outer ? as<FunctionCall>(outer->args[0].get()) : nullptr;
    check("inner is FunctionCall",    inner != nullptr);
    check("inner name == 'add'",      inner && inner->name == "add");
    check("inner has 2 args",         inner && inner->args.size() == 2);
}

// ── Δοκιμές: Binary operators & precedence ────────────────────────────────────

void test_precedence_mul_over_add() {
    std::cout << "\n[Precedence — * πάνω από +]\n";
    // 2 + 3 * 4  →  +(2, *(3, 4))
    auto prog = parse("2 + 3 * 4");

    auto* es  = as<ExprStmt>(stmt(prog, 0));
    auto* add = es ? as<BinaryOp>(es->expr.get()) : nullptr;
    check("outer op == '+'",  add && add->op == "+");

    auto* lhs = add ? as<NumberLiteral>(add->left.get()) : nullptr;
    check("left == 2",        lhs && lhs->value == 2.0);

    auto* mul = add ? as<BinaryOp>(add->right.get()) : nullptr;
    check("right is BinaryOp *",  mul && mul->op == "*");

    auto* ml = mul ? as<NumberLiteral>(mul->left.get())  : nullptr;
    auto* mr = mul ? as<NumberLiteral>(mul->right.get()) : nullptr;
    check("mul left == 3",    ml && ml->value == 3.0);
    check("mul right == 4",   mr && mr->value == 4.0);
}

void test_precedence_parens_override() {
    std::cout << "\n[Precedence — παρενθέσεις]\n";
    // (2 + 3) * 4  →  *(+(2,3), 4)
    auto prog = parse("(2 + 3) * 4");

    auto* es  = as<ExprStmt>(stmt(prog, 0));
    auto* mul = es ? as<BinaryOp>(es->expr.get()) : nullptr;
    check("outer op == '*'",  mul && mul->op == "*");

    auto* inner = mul ? as<BinaryOp>(mul->left.get()) : nullptr;
    check("left is BinaryOp +",  inner && inner->op == "+");

    auto* rhs = mul ? as<NumberLiteral>(mul->right.get()) : nullptr;
    check("right == 4",  rhs && rhs->value == 4.0);
}

void test_equality_operators() {
    std::cout << "\n[Equality operators]\n";
    auto p1 = parse("x == 0");
    auto* es1 = as<ExprStmt>(stmt(p1, 0));
    auto* eq  = es1 ? as<BinaryOp>(es1->expr.get()) : nullptr;
    check("== parsed",  eq && eq->op == "==");

    auto p2 = parse("x != 0");
    auto* es2 = as<ExprStmt>(stmt(p2, 0));
    auto* neq = es2 ? as<BinaryOp>(es2->expr.get()) : nullptr;
    check("!= parsed",  neq && neq->op == "!=");
}

void test_comparison_operators() {
    std::cout << "\n[Comparison operators]\n";
    for (auto [src, expected] : std::initializer_list<std::pair<const char*, const char*>>{
        {"x < 1", "<"}, {"x > 1", ">"}, {"x <= 1", "<="}, {"x >= 1", ">="}}) {
        auto prog = parse(src);
        auto* es  = as<ExprStmt>(stmt(prog, 0));
        auto* bin = es ? as<BinaryOp>(es->expr.get()) : nullptr;
        check(std::string(expected) + " parsed",  bin && bin->op == expected);
    }
}

// ── Δοκιμές: Unary operators ──────────────────────────────────────────────────

void test_unary_minus() {
    std::cout << "\n[UnaryOp — minus]\n";
    auto prog = parse("-5");

    auto* es    = as<ExprStmt>(stmt(prog, 0));
    auto* unary = es ? as<UnaryOp>(es->expr.get()) : nullptr;
    check("is UnaryOp",    unary != nullptr);
    check("op == '-'",     unary && unary->op == "-");
    auto* num = unary ? as<NumberLiteral>(unary->operand.get()) : nullptr;
    check("operand == 5",  num && num->value == 5.0);
}

void test_unary_not() {
    std::cout << "\n[UnaryOp — not]\n";
    auto prog = parse("!flag");

    auto* es    = as<ExprStmt>(stmt(prog, 0));
    auto* unary = es ? as<UnaryOp>(es->expr.get()) : nullptr;
    check("is UnaryOp",     unary != nullptr);
    check("op == '!'",      unary && unary->op == "!");
    auto* id = unary ? as<Identifier>(unary->operand.get()) : nullptr;
    check("operand == 'flag'",  id && id->name == "flag");
}

// ── Δοκιμές: Assignment ───────────────────────────────────────────────────────

void test_assignment() {
    std::cout << "\n[Assignment]\n";
    auto prog = parse("x = 42");

    auto* es     = as<ExprStmt>(stmt(prog, 0));
    auto* assign = es ? as<Assignment>(es->expr.get()) : nullptr;
    check("is Assignment",  assign != nullptr);
    check("name == 'x'",    assign && assign->name == "x");
    auto* val = assign ? as<NumberLiteral>(assign->value.get()) : nullptr;
    check("value == 42",    val && val->value == 42.0);
}

void test_assignment_right_assoc() {
    std::cout << "\n[Assignment — δεξιά-αριστερά]\n";
    // x = y = 5  →  x = (y = 5)
    auto prog = parse("x = y = 5");

    auto* es      = as<ExprStmt>(stmt(prog, 0));
    auto* outer   = es ? as<Assignment>(es->expr.get()) : nullptr;
    check("outer is Assignment",  outer != nullptr);
    check("outer.name == 'x'",    outer && outer->name == "x");

    auto* inner = outer ? as<Assignment>(outer->value.get()) : nullptr;
    check("inner is Assignment",  inner != nullptr);
    check("inner.name == 'y'",    inner && inner->name == "y");
    auto* num = inner ? as<NumberLiteral>(inner->value.get()) : nullptr;
    check("inner value == 5",     num && num->value == 5.0);
}

// ── Δοκιμές: IfStmt ───────────────────────────────────────────────────────────

void test_if_no_else() {
    std::cout << "\n[IfStmt — χωρίς else]\n";
    auto prog = parse("if (x == 0) { return 1 }");

    auto* ifn = as<IfStmt>(stmt(prog, 0));
    check("is IfStmt",          ifn != nullptr);

    auto* cond = ifn ? as<BinaryOp>(ifn->condition.get()) : nullptr;
    check("condition is ==",    cond && cond->op == "==");
    check("thenBody has 1 stmt", ifn && ifn->thenBody.size() == 1);
    check("elseBody is empty",   ifn && ifn->elseBody.empty());
}

void test_if_with_else() {
    std::cout << "\n[IfStmt — με else]\n";
    auto prog = parse("if (x > 0) { return x } else { return 0 }");

    auto* ifn = as<IfStmt>(stmt(prog, 0));
    check("is IfStmt",           ifn != nullptr);
    check("thenBody has 1 stmt", ifn && ifn->thenBody.size() == 1);
    check("elseBody has 1 stmt", ifn && ifn->elseBody.size() == 1);

    auto* elseRet = ifn ? as<ReturnStmt>(ifn->elseBody[0].get()) : nullptr;
    check("else body is ReturnStmt", elseRet != nullptr);
    auto* zero = elseRet ? as<NumberLiteral>(elseRet->value.get()) : nullptr;
    check("else returns 0",          zero && zero->value == 0.0);
}

// ── Δοκιμές: WhileStmt ────────────────────────────────────────────────────────

void test_while_stmt() {
    std::cout << "\n[WhileStmt]\n";
    auto prog = parse("while (i > 0) { i = i - 1 }");

    auto* wh = as<WhileStmt>(stmt(prog, 0));
    check("is WhileStmt",       wh != nullptr);

    auto* cond = wh ? as<BinaryOp>(wh->condition.get()) : nullptr;
    check("condition is >",     cond && cond->op == ">");
    check("body has 1 stmt",    wh && wh->body.size() == 1);

    auto* body_stmt = wh ? as<ExprStmt>(wh->body[0].get()) : nullptr;
    auto* assign    = body_stmt ? as<Assignment>(body_stmt->expr.get()) : nullptr;
    check("body is Assignment", assign != nullptr);
    check("assign name == 'i'", assign && assign->name == "i");
}

// ── Δοκιμή: Πλήρες πρόγραμμα Nova ────────────────────────────────────────────

void test_full_program() {
    std::cout << "\n[Full Nova Program]\n";
    std::string code = R"(
fn add(a: int, b: int) -> int {
    return a + b
}

let result = add(3, 5)
)";
    auto prog = parse(code);

    check("2 statements",          prog->statements.size() == 2);

    auto* fn = as<FunctionDecl>(stmt(prog, 0));
    check("stmt[0] is FunctionDecl", fn != nullptr);
    check("fn name == 'add'",        fn && fn->name == "add");
    check("fn has 2 params",         fn && fn->params.size() == 2);
    check("fn returnType == 'int'",  fn && fn->returnType == "int");
    check("fn body has 1 stmt",      fn && fn->body.size() == 1);

    auto* decl = as<VarDecl>(stmt(prog, 1));
    check("stmt[1] is VarDecl",      decl != nullptr);
    check("let name == 'result'",    decl && decl->name == "result");

    auto* call = decl ? as<FunctionCall>(decl->value.get()) : nullptr;
    check("value is FunctionCall",   call != nullptr);
    check("call name == 'add'",      call && call->name == "add");
    check("call has 2 args",         call && call->args.size() == 2);

    auto* arg0 = call ? as<NumberLiteral>(call->args[0].get()) : nullptr;
    auto* arg1 = call ? as<NumberLiteral>(call->args[1].get()) : nullptr;
    check("arg[0] == 3",             arg0 && arg0->value == 3.0);
    check("arg[1] == 5",             arg1 && arg1->value == 5.0);
}

// ── Entry point ────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Nova Parser Tests ===\n";

    test_var_decl_number();
    test_var_decl_with_type();
    test_var_decl_string();
    test_var_decl_bool();
    test_function_decl();
    test_function_no_return_type();
    test_return_with_value();
    test_return_no_value();
    test_function_call_with_args();
    test_function_call_no_args();
    test_nested_call();
    test_precedence_mul_over_add();
    test_precedence_parens_override();
    test_equality_operators();
    test_comparison_operators();
    test_unary_minus();
    test_unary_not();
    test_assignment();
    test_assignment_right_assoc();
    test_if_no_else();
    test_if_with_else();
    test_while_stmt();
    test_full_program();

    std::cout << "\n═══════════════════════\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << "\n";

    return failed == 0 ? 0 : 1;
}
