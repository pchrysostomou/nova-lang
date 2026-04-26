#include "codegen.h"
#include <stdexcept>

// ── Emit helpers ───────────────────────────────────────────────────────────────

int CodeGen::emit(Op op, int a, int b) {
    return current_->emit(op, a, b);
}

int CodeGen::emitConst(Value v) {
    int idx = current_->addConst(std::move(v));
    return emit(Op::PUSH_CONST, idx);
}

int CodeGen::emitLoad(const std::string& name) {
    return emit(Op::LOAD, current_->addName(name));
}

int CodeGen::emitStore(const std::string& name) {
    return emit(Op::STORE, current_->addName(name));
}

void CodeGen::patch(int instrIdx, int target) {
    current_->patchJump(instrIdx, target);
}

int CodeGen::currentSize() const {
    return current_->size();
}

// ── Entry point ────────────────────────────────────────────────────────────────

std::vector<Chunk> CodeGen::generate(Program* program) {
    main_.name = "__main__";
    current_   = &main_;

    for (const auto& node : program->statements)
        genStatement(node.get());

    emit(Op::HALT);

    std::vector<Chunk> result;
    result.push_back(std::move(main_));
    for (auto& fn : funcs_) result.push_back(std::move(fn));
    return result;
}

// ── Statements ─────────────────────────────────────────────────────────────────

void CodeGen::genStatement(ASTNode* node) {
    switch (node->kind) {
        case NodeKind::VarDecl:      genVarDecl(static_cast<VarDecl*>(node));             break;
        case NodeKind::FunctionDecl: genFunctionDecl(static_cast<FunctionDecl*>(node));   break;
        case NodeKind::ReturnStmt:   genReturnStmt(static_cast<ReturnStmt*>(node));       break;
        case NodeKind::IfStmt:       genIfStmt(static_cast<IfStmt*>(node));               break;
        case NodeKind::WhileStmt:    genWhileStmt(static_cast<WhileStmt*>(node));         break;
        case NodeKind::ForStmt:      genForStmt(static_cast<ForStmt*>(node));             break;
        case NodeKind::ExprStmt:     genExprStmt(static_cast<ExprStmt*>(node));           break;
        default: break;
    }
}

// let name = expr  /  let name: type = expr
void CodeGen::genVarDecl(VarDecl* node) {
    genExpr(node->value.get());   // αφήνει την τιμή στο stack
    emitStore(node->name);        // αφαιρεί από stack, αποθηκεύει
}

// fn name(params) { body }
// Δημιουργεί νέο Chunk για τη συνάρτηση.
// Το __main__ δεν εκτελεί τη συνάρτηση — απλά την ορίζει.
void CodeGen::genFunctionDecl(FunctionDecl* node) {
    // Δημιουργία νέου chunk (προσοχή: emplace_back μπορεί να κάνει realloc —
    // γι' αυτό το current_ επαναφέρεται ΠΡΙΝ το emplace_back)
    funcs_.emplace_back();
    Chunk& fn  = funcs_.back();
    fn.name    = node->name;
    for (const auto& p : node->params) fn.params.push_back(p.name);

    // Εναλλαγή "τρέχοντος chunk"
    Chunk* prev = current_;
    current_    = &fn;

    for (const auto& s : node->body)
        genStatement(s.get());

    // Εγγύηση void return αν δεν υπάρχει ήδη return (π.χ. void functions)
    emit(Op::RETURN);

    current_ = prev;  // επαναφορά στο __main__
}

// return expr  /  return
void CodeGen::genReturnStmt(ReturnStmt* node) {
    if (node->value) {
        genExpr(node->value.get());
        emit(Op::RETURN_VAL);
    } else {
        emit(Op::RETURN);
    }
}

// if (cond) { then } else { else }
//
// Bytecode layout:
//   <cond>
//   JUMP_IF_FALSE → else_start (or end)
//   <then body>
//   JUMP → end          ← μόνο αν υπάρχει else
//   <else body>
//   <end>
void CodeGen::genIfStmt(IfStmt* node) {
    genExpr(node->condition.get());

    int jumpToElse = emit(Op::JUMP_IF_FALSE, 0);   // placeholder

    for (const auto& s : node->thenBody)
        genStatement(s.get());

    if (!node->elseBody.empty()) {
        int jumpToEnd = emit(Op::JUMP, 0);          // placeholder
        patch(jumpToElse, currentSize());            // else_start = here

        for (const auto& s : node->elseBody)
            genStatement(s.get());

        patch(jumpToEnd, currentSize());             // end = here
    } else {
        patch(jumpToElse, currentSize());            // end = here (no else)
    }
}

// while (cond) { body }
//
// Bytecode layout:
//   <loop_start>:
//   <cond>
//   JUMP_IF_FALSE → after_loop
//   <body>
//   JUMP → loop_start
//   <after_loop>:
void CodeGen::genWhileStmt(WhileStmt* node) {
    int loopStart = currentSize();                  // index of condition start

    genExpr(node->condition.get());

    int jumpOut = emit(Op::JUMP_IF_FALSE, 0);       // placeholder

    for (const auto& s : node->body)
        genStatement(s.get());

    emit(Op::JUMP, loopStart);                      // πίσω στην αρχή
    patch(jumpOut, currentSize());                   // after_loop = here
}

// for (init; cond; update) { body }
//
// Bytecode layout:
//   <init>                   (VarDecl or assignment, or nothing)
//   <loop_start>:
//   <cond>                   (or PUSH true if no condition)
//   JUMP_IF_FALSE → after
//   <body>
//   <update>                 (assignment — value discarded)
//   JUMP → loop_start
//   <after>:
void CodeGen::genForStmt(ForStmt* node) {
    if (node->init) genStatement(node->init.get());

    int loopStart = currentSize();

    if (node->condition) {
        genExpr(node->condition.get());
    } else {
        emitConst(Value::Bool(true));
    }

    int jumpOut = emit(Op::JUMP_IF_FALSE, 0);

    for (const auto& s : node->body)
        genStatement(s.get());

    // update: treat like ExprStmt (Assignment → STORE with no DUP; other → POP)
    if (node->update) {
        if (node->update->kind == NodeKind::Assignment) {
            auto* assign = static_cast<Assignment*>(node->update.get());
            genExpr(assign->value.get());
            emitStore(assign->name);
        } else {
            genExpr(node->update.get());
            emit(Op::POP);
        }
    }

    emit(Op::JUMP, loopStart);
    patch(jumpOut, currentSize());
}

// Έκφραση ως statement:
//   - Assignment: δεν χρειάζεται POP (STORE αφαιρεί από stack)
//   - Οτιδήποτε άλλο: γεννάει τιμή → POP για να μην μένει στο stack
void CodeGen::genExprStmt(ExprStmt* node) {
    if (node->expr->kind == NodeKind::Assignment) {
        auto* assign = static_cast<Assignment*>(node->expr.get());
        genExpr(assign->value.get());
        emitStore(assign->name);
    } else {
        genExpr(node->expr.get());
        emit(Op::POP);
    }
}

// ── Expressions ────────────────────────────────────────────────────────────────
// Κάθε genExpr αφήνει ΑΚΡΙΒΩς 1 value στο stack.

void CodeGen::genExpr(ASTNode* node) {
    switch (node->kind) {
        case NodeKind::NumberLiteral: {
            auto* n = static_cast<NumberLiteral*>(node);
            if (n->value == (long long)n->value)
                emitConst(Value::Int((long long)n->value));
            else
                emitConst(Value::Float(n->value));
            break;
        }
        case NodeKind::StringLiteral:
            emitConst(Value::String(static_cast<StringLiteral*>(node)->value));
            break;
        case NodeKind::BoolLiteral:
            emitConst(Value::Bool(static_cast<BoolLiteral*>(node)->value));
            break;
        case NodeKind::Identifier:
            emitLoad(static_cast<Identifier*>(node)->name);
            break;
        case NodeKind::BinaryOp:
            genBinaryOp(static_cast<BinaryOp*>(node));
            break;
        case NodeKind::UnaryOp:
            genUnaryOp(static_cast<UnaryOp*>(node));
            break;
        case NodeKind::FunctionCall:
            genFunctionCall(static_cast<FunctionCall*>(node));
            break;
        case NodeKind::Assignment:
            genAssignmentExpr(static_cast<Assignment*>(node));
            break;
        case NodeKind::ArrayLiteral: {
            auto* arr = static_cast<ArrayLiteral*>(node);
            for (const auto& e : arr->elements)
                genExpr(e.get());
            emit(Op::ARRAY_NEW, (int)arr->elements.size());
            break;
        }
        case NodeKind::IndexExpr: {
            auto* ie = static_cast<IndexExpr*>(node);
            genExpr(ie->array.get());
            genExpr(ie->index.get());
            emit(Op::ARRAY_GET);
            break;
        }
        case NodeKind::IndexAssign: {
            auto* ia = static_cast<IndexAssign*>(node);
            genExpr(ia->array.get());
            genExpr(ia->index.get());
            genExpr(ia->value.get());
            emit(Op::ARRAY_SET);
            // ARRAY_SET mutates through shared_ptr; push the new value as result
            genExpr(ia->array.get());
            genExpr(ia->index.get());
            emit(Op::ARRAY_GET);
            break;
        }
        default:
            throw std::runtime_error("CodeGen: unknown expression node");
    }
}

void CodeGen::genBinaryOp(BinaryOp* node) {
    genExpr(node->left.get());    // push left
    genExpr(node->right.get());   // push right

    static const std::unordered_map<std::string, Op> OPS = {
        {"+", Op::ADD}, {"-", Op::SUB}, {"*", Op::MUL}, {"/", Op::DIV}, {"%", Op::MOD},
        {"==", Op::EQ}, {"!=", Op::NEQ},
        {"<",  Op::LT}, {">",  Op::GT}, {"<=", Op::LEQ}, {">=", Op::GEQ},
    };
    auto it = OPS.find(node->op);
    if (it == OPS.end())
        throw std::runtime_error("CodeGen: unknown operator '" + node->op + "'");
    emit(it->second);
}

void CodeGen::genUnaryOp(UnaryOp* node) {
    genExpr(node->operand.get());
    if (node->op == "-") emit(Op::NEG);
    else if (node->op == "!") emit(Op::NOT);
    else throw std::runtime_error("CodeGen: unknown unary op '" + node->op + "'");
}

// foo(a, b):
//   push a
//   push b
//   CALL "foo" 2
// Αφήνει την επιστρεφόμενη τιμή στο stack (ή Void αν void fn).
void CodeGen::genFunctionCall(FunctionCall* node) {
    for (const auto& arg : node->args)
        genExpr(arg.get());
    int nameIdx = current_->addName(node->name);
    emit(Op::CALL, nameIdx, (int)node->args.size());
}

// x = expr (ως έκφραση — αφήνει την τιμή στο stack)
// DUP → STORE (ένα αντίγραφο για αποθήκευση, ένα παραμένει στο stack)
void CodeGen::genAssignmentExpr(Assignment* node) {
    genExpr(node->value.get());
    emit(Op::DUP);
    emitStore(node->name);
}
