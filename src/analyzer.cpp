#include "analyzer.h"
#include <string>
#include <sstream>

// ── Errors ─────────────────────────────────────────────────────────────────────

void Analyzer::addError(const std::string& msg, int line) {
    errors_.push_back({msg, line});
}

// ── Scope management ───────────────────────────────────────────────────────────

void Analyzer::pushScope() { scopes_.push_back({}); }
void Analyzer::popScope()  { scopes_.pop_back(); }

void Analyzer::declare(const std::string& name, const std::string& type, int line) {
    if (inCurrentScope(name)) {
        addError("'" + name + "' already declared in this scope", line);
        return;
    }
    scopes_.back()[name] = {type, line};
}

bool Analyzer::inCurrentScope(const std::string& name) const {
    return !scopes_.empty() && scopes_.back().count(name) > 0;
}

// Ψάχνει από το εσωτερικό scope προς τα έξω
const Analyzer::Symbol* Analyzer::lookup(const std::string& name) const {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return &it->second;
    }
    return nullptr;
}

// ── Type helpers ───────────────────────────────────────────────────────────────

bool Analyzer::isNumeric(const std::string& t) const {
    return t == "int" || t == "float";
}

// Δύο τύποι είναι συμβατοί αν:
//   - είναι ίδιοι
//   - κάποιος είναι "unknown" (αποφυγή cascading errors)
//   - και οι δύο είναι αριθμητικοί (int/float interop)
bool Analyzer::typesCompatible(const std::string& a, const std::string& b) const {
    if (a == b)          return true;
    if (a == "unknown" || b == "unknown") return true;
    if (isNumeric(a) && isNumeric(b))     return true;
    return false;
}

// ── Built-ins & function collection ───────────────────────────────────────────

void Analyzer::registerBuiltins() {
    // print(x) — δέχεται οποιονδήποτε τύπο, δεν επιστρέφει τιμή
    functions_["print"] = {{}, "void", 0, /*variadic=*/true};
}

// Πρώτο πέρασμα: καταγράφει όλες τις fn δηλώσεις πριν αναλύσουμε τα bodies.
// Χωρίς αυτό, δεν θα μπορούσαμε να καλούμε συνάρτηση πριν ορισθεί.
void Analyzer::collectFunctions(Program* program) {
    for (const auto& node : program->statements) {
        if (node->kind != NodeKind::FunctionDecl) continue;

        auto* fn = static_cast<FunctionDecl*>(node.get());

        if (functions_.count(fn->name)) {
            addError("function '" + fn->name + "' already declared", fn->line);
            continue;
        }

        FnSig sig;
        for (const auto& p : fn->params) sig.paramTypes.push_back(p.type);
        sig.returnType = fn->returnType.empty() ? "void" : fn->returnType;
        sig.line       = fn->line;
        functions_[fn->name] = std::move(sig);
    }
}

// ── blockReturns ───────────────────────────────────────────────────────────────
//
// Ελέγχει αν ένα block επιστρέφει ΣΙΓΟΥΡΑ σε όλα τα execution paths.
// Χρησιμοποιείται για τον έλεγχο missing return.
//
// Ένα block "σίγουρα επιστρέφει" αν:
//   - Περιέχει ένα ReturnStmt, Ή
//   - Περιέχει ένα if/else όπου ΚΑΙ τα δύο branches σίγουρα επιστρέφουν

bool Analyzer::blockReturns(const NodeList& stmts) const {
    for (const auto& node : stmts) {
        if (node->kind == NodeKind::ReturnStmt) return true;

        if (node->kind == NodeKind::IfStmt) {
            auto* ifn = static_cast<const IfStmt*>(node.get());
            if (!ifn->elseBody.empty() &&
                blockReturns(ifn->thenBody) &&
                blockReturns(ifn->elseBody)) {
                return true;
            }
        }
    }
    return false;
}

// ── Entry point ────────────────────────────────────────────────────────────────

bool Analyzer::analyze(Program* program) {
    errors_.clear();
    scopes_.clear();
    functions_.clear();
    currentReturnType_ = "";

    registerBuiltins();
    collectFunctions(program);   // πρώτο πέρασμα

    pushScope();                 // global scope
    for (const auto& node : program->statements)
        visitStatement(node.get());
    popScope();

    return errors_.empty();
}

// ── Statement visitors ─────────────────────────────────────────────────────────

void Analyzer::visitStatement(ASTNode* node) {
    switch (node->kind) {
        case NodeKind::VarDecl:      visitVarDecl(static_cast<VarDecl*>(node));             break;
        case NodeKind::FunctionDecl: visitFunctionDecl(static_cast<FunctionDecl*>(node));   break;
        case NodeKind::ReturnStmt:   visitReturnStmt(static_cast<ReturnStmt*>(node));       break;
        case NodeKind::IfStmt:       visitIfStmt(static_cast<IfStmt*>(node));               break;
        case NodeKind::WhileStmt:    visitWhileStmt(static_cast<WhileStmt*>(node));         break;
        case NodeKind::ForStmt:      visitForStmt(static_cast<ForStmt*>(node));             break;
        case NodeKind::ExprStmt:     visitExprStmt(static_cast<ExprStmt*>(node));           break;
        default: break;
    }
}

// let name = expr         → τύπος inferrd από expr
// let name: type = expr   → ελέγχει ότι ο τύπος του expr είναι συμβατός
void Analyzer::visitVarDecl(VarDecl* node) {
    auto valType = inferType(node->value.get());

    if (!node->type.empty() && !typesCompatible(node->type, valType)) {
        addError("type mismatch: '" + node->name + "' declared as '" + node->type +
                 "' but initializer is '" + valType + "'", node->line);
    }

    // Αν έχει explicit τύπο, τον χρησιμοποιεί· αλλιώς inferred
    auto finalType = node->type.empty() ? valType : node->type;
    declare(node->name, finalType, node->line);
}

// fn name(params) -> ret { body }
//   - ανοίγει νέο scope, δηλώνει τα params
//   - αναλύει το body
//   - ελέγχει missing return αν returnType != void
void Analyzer::visitFunctionDecl(FunctionDecl* node) {
    pushScope();

    for (const auto& p : node->params)
        declare(p.name, p.type, node->line);

    auto prevReturn    = currentReturnType_;
    currentReturnType_ = node->returnType.empty() ? "void" : node->returnType;

    for (const auto& s : node->body)
        visitStatement(s.get());

    if (currentReturnType_ != "void" && !blockReturns(node->body)) {
        addError("function '" + node->name +
                 "' missing return statement (return type is '" +
                 currentReturnType_ + "')", node->line);
    }

    currentReturnType_ = prevReturn;
    popScope();
}

void Analyzer::visitReturnStmt(ReturnStmt* node) {
    if (!node->value) {
        // bare return — επιτρέπεται μόνο σε void συναρτήσεις
        if (currentReturnType_ != "void") {
            addError("empty return in non-void function (expected '" +
                     currentReturnType_ + "')", node->line);
        }
        return;
    }

    auto retType = inferType(node->value.get());
    if (!typesCompatible(currentReturnType_, retType)) {
        addError("return type mismatch: expected '" + currentReturnType_ +
                 "' but got '" + retType + "'", node->line);
    }
}

void Analyzer::visitIfStmt(IfStmt* node) {
    inferType(node->condition.get());

    pushScope();
    for (const auto& s : node->thenBody) visitStatement(s.get());
    popScope();

    if (!node->elseBody.empty()) {
        pushScope();
        for (const auto& s : node->elseBody) visitStatement(s.get());
        popScope();
    }
}

void Analyzer::visitWhileStmt(WhileStmt* node) {
    inferType(node->condition.get());

    pushScope();
    for (const auto& s : node->body) visitStatement(s.get());
    popScope();
}

// for (init; cond; update) { body }
// init variable is scoped to the for loop (push/pop scope around the whole thing)
void Analyzer::visitForStmt(ForStmt* node) {
    pushScope();                                          // for-header scope

    if (node->init) visitStatement(node->init.get());
    if (node->condition) inferType(node->condition.get());

    pushScope();                                          // body scope
    for (const auto& s : node->body) visitStatement(s.get());
    popScope();

    if (node->update) inferType(node->update.get());

    popScope();
}

void Analyzer::visitExprStmt(ExprStmt* node) {
    inferType(node->expr.get());
}

// ── Type inference ─────────────────────────────────────────────────────────────
//
// Επιστρέφει τον τύπο της έκφρασης ΚΑΙ ελέγχει για σφάλματα εσωτερικά.
// Επιστρέφει "unknown" όταν δεν μπορεί να προσδιορίσει τον τύπο — αυτό
// κάνει τους downstream ελέγχους να αγνοούν τυχόν cascading errors.

std::string Analyzer::inferType(ASTNode* node) {
    if (!node) return "void";

    switch (node->kind) {

        case NodeKind::NumberLiteral: {
            auto v = static_cast<NumberLiteral*>(node)->value;
            return (v == static_cast<int>(v)) ? "int" : "float";
        }

        case NodeKind::StringLiteral:
            return "string";

        case NodeKind::BoolLiteral:
            return "bool";

        case NodeKind::Identifier: {
            auto* id  = static_cast<Identifier*>(node);
            auto* sym = lookup(id->name);
            if (!sym) {
                addError("undefined variable '" + id->name + "'", id->line);
                return "unknown";
            }
            return sym->type;
        }

        case NodeKind::BinaryOp: {
            auto* bin  = static_cast<BinaryOp*>(node);
            auto  lt   = inferType(bin->left.get());
            auto  rt   = inferType(bin->right.get());
            return binaryResultType(bin->op, lt, rt, bin->line);
        }

        case NodeKind::UnaryOp: {
            auto* u       = static_cast<UnaryOp*>(node);
            auto  opType  = inferType(u->operand.get());

            if (u->op == "-") {
                if (!isNumeric(opType) && opType != "unknown")
                    addError("unary '-' requires a numeric operand, got '" + opType + "'", u->line);
                return opType;
            }
            if (u->op == "!") {
                if (opType != "bool" && opType != "unknown")
                    addError("unary '!' requires a bool operand, got '" + opType + "'", u->line);
                return "bool";
            }
            return "unknown";
        }

        case NodeKind::Assignment: {
            auto* assign = static_cast<Assignment*>(node);
            auto* sym    = lookup(assign->name);
            if (!sym) {
                addError("undefined variable '" + assign->name + "'", assign->line);
                return "unknown";
            }
            auto valType = inferType(assign->value.get());
            if (!typesCompatible(sym->type, valType)) {
                addError("type mismatch: cannot assign '" + valType +
                         "' to '" + assign->name + "' (type '" + sym->type + "')", assign->line);
            }
            return sym->type;
        }

        case NodeKind::FunctionCall: {
            auto* call = static_cast<FunctionCall*>(node);
            auto  it   = functions_.find(call->name);

            if (it == functions_.end()) {
                addError("undefined function '" + call->name + "'", call->line);
                // still infer arg types to catch nested errors
                for (const auto& arg : call->args) inferType(arg.get());
                return "unknown";
            }

            const FnSig& sig = it->second;

            if (sig.variadic) {
                // Built-in variadic (π.χ. print) — ελέγχει μόνο τους τύπους των args
                for (const auto& arg : call->args) inferType(arg.get());
            } else {
                if (call->args.size() != sig.paramTypes.size()) {
                    addError("'" + call->name + "' expects " +
                             std::to_string(sig.paramTypes.size()) +
                             " argument(s), got " +
                             std::to_string(call->args.size()), call->line);
                    // still infer arg types
                    for (const auto& arg : call->args) inferType(arg.get());
                } else {
                    for (size_t i = 0; i < call->args.size(); ++i) {
                        auto argType = inferType(call->args[i].get());
                        if (!typesCompatible(sig.paramTypes[i], argType)) {
                            addError("argument " + std::to_string(i + 1) +
                                     " of '" + call->name + "': expected '" +
                                     sig.paramTypes[i] + "', got '" + argType + "'",
                                     call->line);
                        }
                    }
                }
            }

            return sig.returnType;
        }

        default:
            return "unknown";
    }
}

// ── Binary operator type rules ─────────────────────────────────────────────────

std::string Analyzer::binaryResultType(const std::string& op,
                                        const std::string& lt,
                                        const std::string& rt, int line) {
    // == != : τύποι πρέπει να είναι συμβατοί, αποτέλεσμα bool
    if (op == "==" || op == "!=") {
        if (!typesCompatible(lt, rt))
            addError("cannot compare '" + lt + "' with '" + rt + "' using '" + op + "'", line);
        return "bool";
    }

    // < > <= >= : απαιτούν αριθμητικούς τελεστέους, αποτέλεσμα bool
    if (op == "<" || op == ">" || op == "<=" || op == ">=") {
        if (!isNumeric(lt) && lt != "unknown")
            addError("operator '" + op + "' requires numeric operands, got '" + lt + "'", line);
        if (!isNumeric(rt) && rt != "unknown")
            addError("operator '" + op + "' requires numeric operands, got '" + rt + "'", line);
        return "bool";
    }

    // + : επιτρέπει string + string (concatenation)
    if (op == "+") {
        if ((lt == "string" || lt == "unknown") && (rt == "string" || rt == "unknown"))
            return "string";
    }

    // + - * / % : απαιτούν αριθμητικούς τελεστέους
    if (!isNumeric(lt) && lt != "unknown")
        addError("operator '" + op + "' requires numeric operands, got '" + lt + "'", line);
    if (!isNumeric(rt) && rt != "unknown")
        addError("operator '" + op + "' requires numeric operands, got '" + rt + "'", line);

    // Αν κάποιος τελεστέος είναι float, το αποτέλεσμα είναι float
    if (lt == "float" || rt == "float") return "float";
    return "int";
}
