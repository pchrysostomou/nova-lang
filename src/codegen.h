#pragma once
#include "ast.h"
#include "vm.h"
#include <vector>

// ── CodeGen ───────────────────────────────────────────────────────────────────
//
// Μετατρέπει το AST σε bytecode (Chunks) που εκτελεί το VM.
//
// Κάθε συνάρτηση γίνεται ένα Chunk.
// Top-level κώδικας γίνεται το "__main__" Chunk.
//
// Κατά τη διάρκεια της παραγωγής, η `current_` δείχνει στο Chunk
// που χτίζεται αυτή τη στιγμή.

class CodeGen {
public:
    // Επιστρέφει λίστα Chunks: [__main__, fn1, fn2, ...]
    std::vector<Chunk> generate(Program* program);

private:
    Chunk              main_;   // top-level chunk
    std::vector<Chunk> funcs_;  // user-defined functions
    Chunk*             current_ = nullptr;

    // ── Emit helpers ──────────────────────────────────────────────────────────
    int  emit(Op op, int a = 0, int b = 0);
    int  emitConst(Value v);          // PUSH_CONST με νέα σταθερά
    int  emitLoad(const std::string& name);
    int  emitStore(const std::string& name);
    void patch(int instrIdx, int target);
    int  currentSize() const;

    // ── Statement generators ──────────────────────────────────────────────────
    void genStatement(ASTNode* node);
    void genVarDecl(VarDecl* node);
    void genFunctionDecl(FunctionDecl* node);
    void genReturnStmt(ReturnStmt* node);
    void genIfStmt(IfStmt* node);
    void genWhileStmt(WhileStmt* node);
    void genForStmt(ForStmt* node);
    void genExprStmt(ExprStmt* node);

    // ── Expression generators (κάθε μια αφήνει 1 value στο stack) ────────────
    void genExpr(ASTNode* node);
    void genBinaryOp(BinaryOp* node);
    void genUnaryOp(UnaryOp* node);
    void genFunctionCall(FunctionCall* node);
    void genAssignmentExpr(Assignment* node);
};
