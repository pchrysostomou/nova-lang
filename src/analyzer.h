#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <unordered_map>

// ── Σφάλμα ανάλυσης ───────────────────────────────────────────────────────────
//
// Ο analyzer ΔΕΝ ρίχνει exception — συλλέγει όλα τα σφάλματα ώστε να τα
// αναφέρει μαζί στον χρήστη (σαν πραγματικός compiler).

struct AnalysisError {
    std::string message;
    int         line;
};

// ── Semantic Analyzer ─────────────────────────────────────────────────────────
//
// Περνάει το AST σε δύο στάδια:
//   1. collectFunctions — καταγράφει όλες τις συναρτήσεις (ώστε να μπορούν
//      να καλούνται πριν ορισθούν)
//   2. visit* — ελέγχει κάθε κόμβο για σημασιολογικά σφάλματα

class Analyzer {
public:
    // Επιστρέφει true αν δεν βρέθηκαν σφάλματα
    bool analyze(Program* program);

    const std::vector<AnalysisError>& errors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    // ── Σφάλματα ──────────────────────────────────────────────────────────────
    std::vector<AnalysisError> errors_;
    void addError(const std::string& msg, int line);

    // ── Scope stack ───────────────────────────────────────────────────────────
    //
    // Κάθε scope είναι ένα map: όνομα → {τύπος, γραμμή δήλωσης}.
    // Το lookup ψάχνει από το εσωτερικό προς το εξωτερικό scope.
    // Αυτό επιτρέπει shadowing (ίδιο όνομα σε εσωτερικό scope).

    struct Symbol { std::string type; int line; };
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;

    void          pushScope();
    void          popScope();
    void          declare(const std::string& name, const std::string& type, int line);
    const Symbol* lookup(const std::string& name) const;
    bool          inCurrentScope(const std::string& name) const;

    // ── Μητρώο συναρτήσεων ────────────────────────────────────────────────────
    struct FnSig {
        std::vector<std::string> paramTypes;
        std::string              returnType;   // "void" αν δεν δηλώθηκε
        int                      line;
        bool                     variadic = false;  // π.χ. print — δέχεται οτιδήποτε
    };
    std::unordered_map<std::string, FnSig> functions_;
    std::string currentReturnType_;   // return type της συνάρτησης που αναλύουμε

    // ── Visitors ──────────────────────────────────────────────────────────────
    void        visitStatement(ASTNode* node);
    void        visitVarDecl(VarDecl* node);
    void        visitFunctionDecl(FunctionDecl* node);
    void        visitReturnStmt(ReturnStmt* node);
    void        visitIfStmt(IfStmt* node);
    void        visitWhileStmt(WhileStmt* node);
    void        visitForStmt(ForStmt* node);
    void        visitExprStmt(ExprStmt* node);

    // Επιστρέφει τον τύπο μιας έκφρασης (και ελέγχει σφάλματα εσωτερικά)
    std::string inferType(ASTNode* node);

    // ── Βοηθητικές ────────────────────────────────────────────────────────────
    bool        blockReturns(const NodeList& stmts) const;
    std::string binaryResultType(const std::string& op,
                                 const std::string& lt,
                                 const std::string& rt, int line);
    bool        typesCompatible(const std::string& a, const std::string& b) const;
    bool        isNumeric(const std::string& t) const;
    void        registerBuiltins();
    void        collectFunctions(Program* program);
};
