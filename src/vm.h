#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <stdexcept>

// ── Value ─────────────────────────────────────────────────────────────────────
//
// Κάθε τιμή στη Nova είναι ένα Value.
// Χρησιμοποιούμε tagged union (type + data) αντί για std::variant
// για να είναι κατανοητό σε beginner-level C++.
//
// Arrays: shared_ptr<ArrayValue> — reference semantics.
// ArrayValue is forward-declared here and defined after Value so that
// std::vector<Value> inside ArrayValue compiles with a complete Value type.

struct ArrayValue;   // forward declaration — defined after Value below

enum class ValueType { Int, Float, String, Bool, Void, Array };

struct Value {
    ValueType   type    = ValueType::Void;
    double      num     = 0;       // Int και Float
    bool        boolean = false;   // Bool
    std::string str;               // String
    std::shared_ptr<ArrayValue> array;  // Array (null for non-array Values)

    static Value Int(long long v)       { Value r; r.type = ValueType::Int;    r.num     = (double)v;    return r; }
    static Value Float(double v)        { Value r; r.type = ValueType::Float;  r.num     = v;            return r; }
    static Value String(std::string v)  { Value r; r.type = ValueType::String; r.str     = std::move(v); return r; }
    static Value Bool(bool v)           { Value r; r.type = ValueType::Bool;   r.boolean = v;            return r; }
    static Value Void()                 { return {}; }
    static Value Array(std::vector<Value> elems);  // defined in vm.cpp

    bool        truthy()   const;
    std::string toString() const;
};

// Defined after Value so std::vector<Value> has a complete element type.
struct ArrayValue {
    std::vector<Value> elements;
    explicit ArrayValue(std::vector<Value> e) : elements(std::move(e)) {}
};

// ── Opcodes ───────────────────────────────────────────────────────────────────
//
// Κάθε opcode έχει το ρόλο του:
//   a, b = integer operands (indices into constants[], names[], etc.)

enum class Op {
    // Constants — a = index in chunk.constants
    PUSH_CONST,

    // Variables — a = index in chunk.names
    LOAD,
    STORE,

    // Αριθμητικές πράξεις (binary: pop b, pop a, push result)
    ADD, SUB, MUL, DIV, MOD,
    // Unary
    NEG,   // -x
    NOT,   // !x

    // Συγκρίσεις → bool
    EQ, NEQ, LT, GT, LEQ, GEQ,

    // Control flow — a = target instruction index
    JUMP,            // άνευ όρου
    JUMP_IF_FALSE,   // αν false, πήδα

    // Συναρτήσεις
    // CALL: a = index in chunk.names (function name), b = arg count
    CALL,
    RETURN,      // void return — pushes Void to caller
    RETURN_VAL,  // pop value, return it — pushes value to caller

    // Arrays
    // ARRAY_NEW: a = element count; pops a values (pushed left-to-right), builds array, pushes it
    ARRAY_NEW,
    ARRAY_GET,   // pop index, pop array → push array[index]
    ARRAY_SET,   // pop value, pop index, pop array → array[index] = value (mutates in place)

    // Stack
    POP,    // απόρριψη κορυφής
    DUP,    // αντιγραφή κορυφής

    HALT,   // τερματισμός VM
};

// ── Instruction ───────────────────────────────────────────────────────────────

struct Instr {
    Op  op;
    int a = 0;
    int b = 0;
};

// ── Chunk ─────────────────────────────────────────────────────────────────────
//
// Ένα Chunk = μεταγλωττισμένη συνάρτηση (ή __main__ για top-level κώδικα).
// Περιέχει τις οδηγίες και τα δεδομένα που χρειάζεται για να τρέξει.

struct Chunk {
    std::string              name;       // "__main__" ή όνομα συνάρτησης
    std::vector<std::string> params;     // ονόματα παραμέτρων (για CALL)
    std::vector<Instr>       code;       // bytecode instructions
    std::vector<Value>       constants;  // σταθερές τιμές (αριθμοί, strings)
    std::vector<std::string> names;      // ονόματα μεταβλητών / συναρτήσεων

    int  addConst(Value v);
    int  addName(const std::string& s);  // deduplicates
    int  emit(Op op, int a = 0, int b = 0);
    void patchJump(int instrIdx, int newTarget);
    int  size() const { return (int)code.size(); }
};

// ── VM ────────────────────────────────────────────────────────────────────────
//
// Stack-based virtual machine.
//
// Αρχιτεκτονική:
//   stack_      — value stack (shared across all frames)
//   callStack_  — call stack (ένα Frame ανά function call)
//
// Κάθε Frame έχει:
//   chunk*   — ποια συνάρτηση εκτελείται
//   ip       — instruction pointer (τρέχουσα εντολή)
//   locals   — τοπικές μεταβλητές (map name→Value)
//
// LOAD ψάχνει από innermost frame προς outermost (→ global visibility).
// STORE γράφει πάντα στο ΤΡΕΧΟΝ frame.

class VM {
public:
    explicit VM(std::vector<Chunk> chunks);
    void run();

    // Για testing: ανακατευθύνει το print σε string αντί για stdout
    bool        captureOutput = false;
    std::string capturedOutput;

private:
    std::vector<Chunk> chunks_;
    std::vector<Value> stack_;

    struct Frame {
        const Chunk*                           chunk;
        int                                    ip = 0;
        std::unordered_map<std::string, Value> locals;
    };
    std::vector<Frame> callStack_;

    // Stack operations
    void  push(Value v);
    Value pop();
    Value peek(int depth = 0) const;

    // Variable access
    Value loadVar(const std::string& name) const;
    void  storeVar(const std::string& name, Value v);

    // Helpers
    const Chunk& findChunk(const std::string& name) const;
    void         printValue(const Value& v);
    Value        applyBinaryOp(Op op, Value a, Value b);
};
