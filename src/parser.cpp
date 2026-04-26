#include "parser.h"
#include <stdexcept>
#include <sstream>

// ── Constructor ────────────────────────────────────────────────────────────────

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

// ── Token utilities ────────────────────────────────────────────────────────────

const Token& Parser::current() const {
    return tokens[pos];
}

// Κοιτάει μπροστά χωρίς να καταναλώνει — επιστρέφει EOF αν φύγουμε εκτός
const Token& Parser::peekAt(int offset) const {
    size_t p = pos + offset;
    return (p < tokens.size()) ? tokens[p] : tokens.back();
}

// Επιστρέφει τον τρέχοντα token και προχωράει
Token Parser::consume() {
    return tokens[pos++];
}

// Όπως consume αλλά πετάει σφάλμα αν ο τύπος δεν ταιριάζει
Token Parser::expect(TokenType type) {
    if (!check(type)) {
        error("Expected '" + tokenTypeToString(type) +
              "' but got '" + current().value +
              "' at line " + std::to_string(current().line));
    }
    return consume();
}

bool Parser::check(TokenType type) const {
    return pos < tokens.size() && tokens[pos].type == type;
}

// Καταναλώνει τον token μόνο αν ταιριάζει· επιστρέφει true αν ταίριαξε
bool Parser::match(TokenType type) {
    if (check(type)) { consume(); return true; }
    return false;
}

// Αγνοεί προαιρετικό semicolon (η Nova δεν τα απαιτεί)
void Parser::skipSemicolon() {
    match(TokenType::SEMICOLON);
}

// Ελέγχει αν ο τρέχων token μπορεί να ξεκινήσει έκφραση
bool Parser::canStartExpression() const {
    switch (current().type) {
        case TokenType::NUMBER:
        case TokenType::STRING:
        case TokenType::TRUE_KW:
        case TokenType::FALSE_KW:
        case TokenType::IDENTIFIER:
        case TokenType::LPAREN:
        case TokenType::MINUS:
        case TokenType::BANG:
            return true;
        default:
            return false;
    }
}

void Parser::error(const std::string& msg) const {
    throw std::runtime_error(msg);
}

// ── Helpers ────────────────────────────────────────────────────────────────────

// Ένα type name είναι απλά ένα IDENTIFIER (π.χ. "int", "string", "bool")
std::string Parser::parseTypeName() {
    return expect(TokenType::IDENTIFIER).value;
}

// { statement* }
NodeList Parser::parseBlock() {
    expect(TokenType::LBRACE);
    NodeList stmts;
    while (!check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        stmts.push_back(parseStatement());
    }
    expect(TokenType::RBRACE);
    return stmts;
}

// (name: type, name: type, ...)  — χωρίς τις παρενθέσεις
std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    if (check(TokenType::RPAREN)) return params;   // κενή λίστα

    do {
        Param p;
        p.name = expect(TokenType::IDENTIFIER).value;
        expect(TokenType::COLON);
        p.type = parseTypeName();
        params.push_back(std::move(p));
    } while (match(TokenType::COMMA));

    return params;
}

// ── Statements ─────────────────────────────────────────────────────────────────

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    while (!check(TokenType::EOF_TOKEN)) {
        program->statements.push_back(parseStatement());
    }
    return program;
}

NodePtr Parser::parseStatement() {
    switch (current().type) {
        case TokenType::LET:    return parseVarDecl();
        case TokenType::FN:     return parseFunctionDecl();
        case TokenType::RETURN: return parseReturnStmt();
        case TokenType::IF:     return parseIfStmt();
        case TokenType::WHILE:  return parseWhileStmt();
        case TokenType::FOR:    return parseForStmt();
        default:                return parseExprStmt();
    }
}

// let name = expr
// let name: type = expr
NodePtr Parser::parseVarDecl() {
    int ln = current().line;
    expect(TokenType::LET);

    auto name = expect(TokenType::IDENTIFIER).value;

    std::string type;
    if (match(TokenType::COLON)) {
        type = parseTypeName();
    }

    expect(TokenType::EQUALS);
    auto value = parseExpression();
    skipSemicolon();

    return std::make_unique<VarDecl>(name, type, std::move(value), ln);
}

// fn name(params) -> returnType { body }
// fn name(params) { body }
NodePtr Parser::parseFunctionDecl() {
    int ln = current().line;
    expect(TokenType::FN);

    auto name = expect(TokenType::IDENTIFIER).value;

    expect(TokenType::LPAREN);
    auto params = parseParams();
    expect(TokenType::RPAREN);

    std::string returnType;
    if (match(TokenType::ARROW)) {
        returnType = parseTypeName();
    }

    auto body = parseBlock();

    return std::make_unique<FunctionDecl>(name, std::move(params),
                                          returnType, std::move(body), ln);
}

// return expr
// return         (bare return, π.χ. σε void fn)
NodePtr Parser::parseReturnStmt() {
    int ln = current().line;
    expect(TokenType::RETURN);

    NodePtr value;
    if (canStartExpression()) {
        value = parseExpression();
    }
    skipSemicolon();

    return std::make_unique<ReturnStmt>(std::move(value), ln);
}

// if (cond) { body } else { body }
NodePtr Parser::parseIfStmt() {
    int ln = current().line;
    expect(TokenType::IF);
    expect(TokenType::LPAREN);
    auto cond = parseExpression();
    expect(TokenType::RPAREN);

    auto thenBody = parseBlock();

    NodeList elseBody;
    if (match(TokenType::ELSE)) {
        elseBody = parseBlock();
    }

    return std::make_unique<IfStmt>(std::move(cond),
                                    std::move(thenBody),
                                    std::move(elseBody), ln);
}

// while (cond) { body }
NodePtr Parser::parseWhileStmt() {
    int ln = current().line;
    expect(TokenType::WHILE);
    expect(TokenType::LPAREN);
    auto cond = parseExpression();
    expect(TokenType::RPAREN);
    auto body = parseBlock();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), ln);
}

// for (init; cond; update) { body }
//   init   = let x = expr  |  expr  |  (empty)
//   cond   = expr           |  (empty → true)
//   update = expr           |  (empty)
NodePtr Parser::parseForStmt() {
    int ln = current().line;
    expect(TokenType::FOR);
    expect(TokenType::LPAREN);

    // init
    NodePtr init;
    if (check(TokenType::LET)) {
        init = parseVarDecl();           // parseVarDecl consumes its trailing ';'
    } else if (check(TokenType::SEMICOLON)) {
        consume();                       // empty init
    } else {
        auto expr = parseExpression();
        expect(TokenType::SEMICOLON);
        init = std::make_unique<ExprStmt>(std::move(expr), ln);
    }

    // condition
    NodePtr cond;
    if (!check(TokenType::SEMICOLON)) {
        cond = parseExpression();
    }
    expect(TokenType::SEMICOLON);

    // update
    NodePtr update;
    if (!check(TokenType::RPAREN)) {
        update = parseExpression();
    }
    expect(TokenType::RPAREN);

    auto body = parseBlock();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond),
                                     std::move(update), std::move(body), ln);
}

// Έκφραση ως statement: add(3,5)  /  x = 42
NodePtr Parser::parseExprStmt() {
    int ln = current().line;
    auto expr = parseExpression();
    skipSemicolon();
    return std::make_unique<ExprStmt>(std::move(expr), ln);
}

// ── Expressions ────────────────────────────────────────────────────────────────
//
// Κάθε επίπεδο καλεί το επόμενο (υψηλότερης προτεραιότητας).
// Αυτή η αλυσίδα εγγυάται ότι * δεσμεύει πιο στενά από +, κ.ο.κ.

NodePtr Parser::parseExpression() {
    return parseAssignment();
}

// Αν βλέπουμε  identifier = expr  → Assignment (δεξιά-αριστερά)
// Αλλιώς       → συνέχισε στη σύγκριση ισότητας
NodePtr Parser::parseAssignment() {
    auto left = parseEquality();

    if (left->kind == NodeKind::Identifier && check(TokenType::EQUALS)) {
        auto* id = static_cast<Identifier*>(left.get());
        int ln   = current().line;
        consume();   // eat '='
        auto right = parseAssignment();   // δεξιά-αριστερά: x = y = 5 → x = (y = 5)
        return std::make_unique<Assignment>(id->name, std::move(right), ln);
    }

    return left;
}

// == !=   (αριστερά-δεξιά)
NodePtr Parser::parseEquality() {
    auto left = parseComparison();

    while (check(TokenType::EQEQ) || check(TokenType::NEQ)) {
        auto op    = consume().value;
        auto right = parseComparison();
        int  ln    = left->line;
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right), ln);
    }

    return left;
}

// < > <= >=   (αριστερά-δεξιά)
NodePtr Parser::parseComparison() {
    auto left = parseAddition();

    while (check(TokenType::LT)  || check(TokenType::GT) ||
           check(TokenType::LEQ) || check(TokenType::GEQ)) {
        auto op    = consume().value;
        auto right = parseAddition();
        int  ln    = left->line;
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right), ln);
    }

    return left;
}

// + -   (αριστερά-δεξιά)
NodePtr Parser::parseAddition() {
    auto left = parseMultiplication();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        auto op    = consume().value;
        auto right = parseMultiplication();
        int  ln    = left->line;
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right), ln);
    }

    return left;
}

// * /   (αριστερά-δεξιά)
NodePtr Parser::parseMultiplication() {
    auto left = parseUnary();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        auto op    = consume().value;
        auto right = parseUnary();
        int  ln    = left->line;
        left = std::make_unique<BinaryOp>(op, std::move(left), std::move(right), ln);
    }

    return left;
}

// - !   (δεξιά-αριστερά, unary prefix)
NodePtr Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::BANG)) {
        int  ln      = current().line;
        auto op      = consume().value;
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(op, std::move(operand), ln);
    }
    return parseCall();
}

// foo(args)  →  FunctionCall
// foo        →  Identifier  (χωρίς κλήση)
NodePtr Parser::parseCall() {
    auto expr = parsePrimary();

    if (expr->kind == NodeKind::Identifier && check(TokenType::LPAREN)) {
        auto* id   = static_cast<Identifier*>(expr.get());
        int   ln   = id->line;
        auto  name = id->name;

        consume();   // eat '('
        NodeList args;
        if (!check(TokenType::RPAREN)) {
            args.push_back(parseExpression());
            while (match(TokenType::COMMA)) {
                args.push_back(parseExpression());
            }
        }
        expect(TokenType::RPAREN);

        return std::make_unique<FunctionCall>(name, std::move(args), ln);
    }

    return expr;
}

// Τα "άτομα" — τιμές που δεν αναλύονται περαιτέρω
NodePtr Parser::parsePrimary() {
    const Token& tok = current();

    if (tok.type == TokenType::NUMBER) {
        double val = std::stod(tok.value);
        int    ln  = tok.line;
        consume();
        return std::make_unique<NumberLiteral>(val, ln);
    }

    if (tok.type == TokenType::STRING) {
        auto val = tok.value;
        int  ln  = tok.line;
        consume();
        return std::make_unique<StringLiteral>(val, ln);
    }

    if (tok.type == TokenType::TRUE_KW) {
        int ln = tok.line;
        consume();
        return std::make_unique<BoolLiteral>(true, ln);
    }

    if (tok.type == TokenType::FALSE_KW) {
        int ln = tok.line;
        consume();
        return std::make_unique<BoolLiteral>(false, ln);
    }

    if (tok.type == TokenType::IDENTIFIER) {
        auto name = tok.value;
        int  ln   = tok.line;
        consume();
        return std::make_unique<Identifier>(name, ln);
    }

    // ( expr )  — ομαδοποίηση, δεν δημιουργεί κόμβο
    if (tok.type == TokenType::LPAREN) {
        consume();
        auto expr = parseExpression();
        expect(TokenType::RPAREN);
        return expr;
    }

    error("Unexpected token '" + tok.value +
          "' at line " + std::to_string(tok.line));
}
