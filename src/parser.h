#pragma once
#include "lexer.h"
#include "ast.h"
#include "error.h"
#include <vector>
#include <memory>
#include <stdexcept>

// Exception thrown internally when the parser hits a syntax error.
// Caught by parse() so it can recover and report multiple errors.
struct ParseError : std::exception {
    std::string msg;
    int line, col;
    ParseError(std::string m, int l, int c)
        : msg(std::move(m)), line(l), col(c) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

// ── Parser ────────────────────────────────────────────────────────────────────
//
// Recursive Descent Parser: παίρνει λίστα Tokens από τον Lexer και χτίζει
// το AST. Κάθε parsing function αντιστοιχεί σε έναν κανόνα της γλώσσας.
//
// Ιεραρχία εκφράσεων (από χαμηλότερη σε υψηλότερη προτεραιότητα):
//   parseExpression
//     └─ parseAssignment      (x = ...)      δεξιά-αριστερά
//         └─ parseEquality    (== !=)
//             └─ parseComparison (< > <= >=)
//                 └─ parseAddition    (+ -)
//                     └─ parseMultiplication (* /)
//                         └─ parseUnary      (- !)
//                             └─ parseCall   foo(args)
//                                 └─ parsePrimary

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Returns the AST (possibly partial if there were errors).
    // Check hasErrors() after parse() to see if any syntax errors occurred.
    std::unique_ptr<Program> parse();

    bool                           hasErrors() const { return !errors_.empty(); }
    const std::vector<Diagnostic>& errors()    const { return errors_; }

private:
    std::vector<Token>    tokens;
    size_t                pos = 0;
    std::vector<Diagnostic> errors_;

    // ── Token utilities ───────────────────────────────────────────────────────
    const Token& current() const;
    const Token& peekAt(int offset = 1) const;
    Token        consume();
    Token        expect(TokenType type);
    bool         check(TokenType type) const;
    bool         match(TokenType type);
    void         skipSemicolon();
    bool         canStartExpression() const;

    // ── Helpers ───────────────────────────────────────────────────────────────
    NodeList           parseBlock();
    std::vector<Param> parseParams();
    std::string        parseTypeName();

    // ── Statement parsers ─────────────────────────────────────────────────────
    NodePtr parseStatement();
    NodePtr parseVarDecl();
    NodePtr parseFunctionDecl();
    NodePtr parseReturnStmt();
    NodePtr parseIfStmt();
    NodePtr parseWhileStmt();
    NodePtr parseForStmt();
    NodePtr parseExprStmt();

    // ── Expression parsers (precedence chain) ─────────────────────────────────
    NodePtr parseExpression();
    NodePtr parseAssignment();
    NodePtr parseEquality();
    NodePtr parseComparison();
    NodePtr parseAddition();
    NodePtr parseMultiplication();
    NodePtr parseUnary();
    NodePtr parseCall();
    NodePtr parsePrimary();

    // ── Error recovery ────────────────────────────────────────────────────────
    [[noreturn]] void error(const std::string& msg) const;
    void              synchronize();
};
