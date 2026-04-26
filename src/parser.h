#pragma once
#include "lexer.h"
#include "ast.h"
#include <vector>
#include <memory>
#include <stdexcept>

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
    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens;
    size_t pos = 0;

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

    // ── Error ─────────────────────────────────────────────────────────────────
    [[noreturn]] void error(const std::string& msg) const;
};
