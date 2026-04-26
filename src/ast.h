#pragma once
#include <memory>
#include <string>
#include <vector>

// ── Τύποι κόμβων ─────────────────────────────────────────────────────────────
//
// Κάθε κόμβος του AST έχει ένα NodeKind. Αυτό μας επιτρέπει να ξέρουμε
// τι είδους κόμβος είναι χωρίς dynamic_cast — χρήσιμο στον interpreter.

enum class NodeKind {
    // Εκφράσεις (παράγουν τιμή)
    NumberLiteral,
    StringLiteral,
    BoolLiteral,
    Identifier,
    BinaryOp,
    UnaryOp,
    FunctionCall,
    Assignment,

    // Statements (εκτελούνται για παρενέργειες)
    VarDecl,
    FunctionDecl,
    ReturnStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    ExprStmt,

    // Δομή
    Program,
};

// ── Βάση ─────────────────────────────────────────────────────────────────────

struct ASTNode {
    NodeKind kind;
    int      line = 0;
    virtual ~ASTNode() = default;
protected:
    ASTNode(NodeKind k, int l = 0) : kind(k), line(l) {}
};

using NodePtr  = std::unique_ptr<ASTNode>;
using NodeList = std::vector<NodePtr>;

// ── Εκφράσεις ────────────────────────────────────────────────────────────────

// 42  /  3.14
struct NumberLiteral : ASTNode {
    double value;
    NumberLiteral(double v, int line = 0)
        : ASTNode(NodeKind::NumberLiteral, line), value(v) {}
};

// "hello"
struct StringLiteral : ASTNode {
    std::string value;
    StringLiteral(std::string v, int line = 0)
        : ASTNode(NodeKind::StringLiteral, line), value(std::move(v)) {}
};

// true  /  false
struct BoolLiteral : ASTNode {
    bool value;
    BoolLiteral(bool v, int line = 0)
        : ASTNode(NodeKind::BoolLiteral, line), value(v) {}
};

// x  /  myVar
struct Identifier : ASTNode {
    std::string name;
    Identifier(std::string n, int line = 0)
        : ASTNode(NodeKind::Identifier, line), name(std::move(n)) {}
};

// a + b  /  x == 0  /  i < 10
struct BinaryOp : ASTNode {
    std::string op;
    NodePtr     left;
    NodePtr     right;
    BinaryOp(std::string op, NodePtr l, NodePtr r, int line = 0)
        : ASTNode(NodeKind::BinaryOp, line), op(std::move(op)),
          left(std::move(l)), right(std::move(r)) {}
};

// -x  /  !flag
struct UnaryOp : ASTNode {
    std::string op;
    NodePtr     operand;
    UnaryOp(std::string op, NodePtr operand, int line = 0)
        : ASTNode(NodeKind::UnaryOp, line), op(std::move(op)),
          operand(std::move(operand)) {}
};

// add(3, 5)  /  print(result)
struct FunctionCall : ASTNode {
    std::string name;
    NodeList    args;
    FunctionCall(std::string name, NodeList args, int line = 0)
        : ASTNode(NodeKind::FunctionCall, line), name(std::move(name)),
          args(std::move(args)) {}
};

// x = 42  (μετά την αρχική δήλωση)
struct Assignment : ASTNode {
    std::string name;
    NodePtr     value;
    Assignment(std::string name, NodePtr value, int line = 0)
        : ASTNode(NodeKind::Assignment, line), name(std::move(name)),
          value(std::move(value)) {}
};

// ── Statements ────────────────────────────────────────────────────────────────

// let x = 42        (type = "")
// let x: int = 42   (type = "int")
struct VarDecl : ASTNode {
    std::string name;
    std::string type;   // άδειο αν δεν δηλώθηκε τύπος
    NodePtr     value;
    VarDecl(std::string name, std::string type, NodePtr value, int line = 0)
        : ASTNode(NodeKind::VarDecl, line), name(std::move(name)),
          type(std::move(type)), value(std::move(value)) {}
};

// Ένα parameter συνάρτησης: name: type
struct Param {
    std::string name;
    std::string type;
};

// fn add(a: int, b: int) -> int { ... }
struct FunctionDecl : ASTNode {
    std::string        name;
    std::vector<Param> params;
    std::string        returnType;   // άδειο αν δεν δηλώθηκε
    NodeList           body;
    FunctionDecl(std::string name, std::vector<Param> params,
                 std::string returnType, NodeList body, int line = 0)
        : ASTNode(NodeKind::FunctionDecl, line), name(std::move(name)),
          params(std::move(params)), returnType(std::move(returnType)),
          body(std::move(body)) {}
};

// return expr  /  return  (χωρίς τιμή)
struct ReturnStmt : ASTNode {
    NodePtr value;   // nullptr αν bare return
    ReturnStmt(NodePtr value, int line = 0)
        : ASTNode(NodeKind::ReturnStmt, line), value(std::move(value)) {}
};

// if (cond) { thenBody } else { elseBody }
struct IfStmt : ASTNode {
    NodePtr  condition;
    NodeList thenBody;
    NodeList elseBody;   // άδειο αν δεν υπάρχει else
    IfStmt(NodePtr cond, NodeList thenB, NodeList elseB, int line = 0)
        : ASTNode(NodeKind::IfStmt, line), condition(std::move(cond)),
          thenBody(std::move(thenB)), elseBody(std::move(elseB)) {}
};

// while (cond) { body }
struct WhileStmt : ASTNode {
    NodePtr  condition;
    NodeList body;
    WhileStmt(NodePtr cond, NodeList body, int line = 0)
        : ASTNode(NodeKind::WhileStmt, line), condition(std::move(cond)),
          body(std::move(body)) {}
};

// for (init; cond; update) { body }
//   init   — VarDecl or ExprStmt (assignment), or nullptr
//   update — Assignment expression, or nullptr
struct ForStmt : ASTNode {
    NodePtr  init;
    NodePtr  condition;
    NodePtr  update;
    NodeList body;
    ForStmt(NodePtr init, NodePtr cond, NodePtr update, NodeList body, int line = 0)
        : ASTNode(NodeKind::ForStmt, line), init(std::move(init)),
          condition(std::move(cond)), update(std::move(update)),
          body(std::move(body)) {}
};

// Έκφραση ως statement: add(3,5)  /  x = 42
struct ExprStmt : ASTNode {
    NodePtr expr;
    ExprStmt(NodePtr expr, int line = 0)
        : ASTNode(NodeKind::ExprStmt, line), expr(std::move(expr)) {}
};

// ── Ρίζα ─────────────────────────────────────────────────────────────────────

struct Program : ASTNode {
    NodeList statements;
    Program() : ASTNode(NodeKind::Program) {}
};
