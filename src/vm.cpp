#include "vm.h"
#include <iostream>
#include <sstream>
#include <cmath>

// ── Value ─────────────────────────────────────────────────────────────────────

Value Value::Array(std::vector<Value> elems) {
    Value r;
    r.type  = ValueType::Array;
    r.array = std::make_shared<ArrayValue>(std::move(elems));
    return r;
}

bool Value::truthy() const {
    switch (type) {
        case ValueType::Bool:   return boolean;
        case ValueType::Int:    return num != 0;
        case ValueType::Float:  return num != 0.0;
        case ValueType::String: return !str.empty();
        case ValueType::Array:  return !array->elements.empty();
        case ValueType::Void:   return false;
    }
    return false;
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::Int: {
            long long iv = (long long)num;
            return std::to_string(iv);
        }
        case ValueType::Float: {
            std::ostringstream oss;
            oss << num;
            std::string s = oss.str();
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos)
                s += ".0";
            return s;
        }
        case ValueType::String: return str;
        case ValueType::Bool:   return boolean ? "true" : "false";
        case ValueType::Array: {
            std::string s = "[";
            for (size_t i = 0; i < array->elements.size(); ++i) {
                if (i > 0) s += ", ";
                s += array->elements[i].toString();
            }
            return s + "]";
        }
        case ValueType::Void:   return "";
    }
    return "";
}

// ── Chunk ─────────────────────────────────────────────────────────────────────

int Chunk::addConst(Value v) {
    constants.push_back(std::move(v));
    return (int)constants.size() - 1;
}

// Deduplication: αν υπάρχει ήδη το όνομα, επιστρέφει τον ίδιο index
int Chunk::addName(const std::string& s) {
    for (int i = 0; i < (int)names.size(); ++i)
        if (names[i] == s) return i;
    names.push_back(s);
    return (int)names.size() - 1;
}

int Chunk::emit(Op op, int a, int b) {
    code.push_back({op, a, b});
    return (int)code.size() - 1;
}

// Χρησιμοποιείται για να "επιδιορθώσουμε" ένα jump αφού ξέρουμε τον target
void Chunk::patchJump(int instrIdx, int newTarget) {
    code[instrIdx].a = newTarget;
}

// ── VM ────────────────────────────────────────────────────────────────────────

VM::VM(std::vector<Chunk> chunks) : chunks_(std::move(chunks)) {}

void VM::push(Value v)            { stack_.push_back(std::move(v)); }
Value VM::pop() {
    if (stack_.empty()) throw std::runtime_error("VM: stack underflow");
    Value v = std::move(stack_.back());
    stack_.pop_back();
    return v;
}
Value VM::peek(int depth) const   { return stack_[stack_.size() - 1 - depth]; }

// LOAD: ψάχνει από innermost frame προς outermost
// Αυτό επιτρέπει στις συναρτήσεις να βλέπουν global μεταβλητές
Value VM::loadVar(const std::string& name) const {
    for (int i = (int)callStack_.size() - 1; i >= 0; --i) {
        auto it = callStack_[i].locals.find(name);
        if (it != callStack_[i].locals.end()) return it->second;
    }
    throw std::runtime_error("VM: undefined variable '" + name + "'");
}

// STORE: γράφει πάντα στο τρέχον (innermost) frame
void VM::storeVar(const std::string& name, Value v) {
    callStack_.back().locals[name] = std::move(v);
}

const Chunk& VM::findChunk(const std::string& name) const {
    for (const auto& c : chunks_)
        if (c.name == name) return c;
    throw std::runtime_error("VM: undefined function '" + name + "'");
}

void VM::printValue(const Value& v) {
    std::string s = v.toString();
    if (captureOutput)
        capturedOutput += s + "\n";
    else
        std::cout << s << "\n";
}

Value VM::applyBinaryOp(Op op, Value a, Value b) {
    bool aIsFloat = (a.type == ValueType::Float);
    bool bIsFloat = (b.type == ValueType::Float);
    bool anyFloat = aIsFloat || bIsFloat;

    switch (op) {
        case Op::ADD:
            if (a.type == ValueType::String)
                return Value::String(a.str + b.str);
            return anyFloat ? Value::Float(a.num + b.num)
                            : Value::Int((long long)a.num + (long long)b.num);
        case Op::SUB:
            return anyFloat ? Value::Float(a.num - b.num)
                            : Value::Int((long long)a.num - (long long)b.num);
        case Op::MUL:
            return anyFloat ? Value::Float(a.num * b.num)
                            : Value::Int((long long)a.num * (long long)b.num);
        case Op::DIV:
            if (b.num == 0) throw std::runtime_error("VM: division by zero");
            return anyFloat ? Value::Float(a.num / b.num)
                            : Value::Int((long long)a.num / (long long)b.num);
        case Op::MOD:
            if (b.num == 0) throw std::runtime_error("VM: modulo by zero");
            return anyFloat ? Value::Float(std::fmod(a.num, b.num))
                            : Value::Int((long long)a.num % (long long)b.num);
        case Op::EQ:
            if (a.type == ValueType::String)  return Value::Bool(a.str == b.str);
            if (a.type == ValueType::Bool)    return Value::Bool(a.boolean == b.boolean);
            return Value::Bool(a.num == b.num);
        case Op::NEQ:
            if (a.type == ValueType::String)  return Value::Bool(a.str != b.str);
            if (a.type == ValueType::Bool)    return Value::Bool(a.boolean != b.boolean);
            return Value::Bool(a.num != b.num);
        case Op::LT:  return Value::Bool(a.num <  b.num);
        case Op::GT:  return Value::Bool(a.num >  b.num);
        case Op::LEQ: return Value::Bool(a.num <= b.num);
        case Op::GEQ: return Value::Bool(a.num >= b.num);
        default:      throw std::runtime_error("VM: unknown binary op");
    }
}

// ── Main execution loop ────────────────────────────────────────────────────────
//
// Κάθε iteration:
//   1. Παίρνει το τρέχον frame (callStack_.back())
//   2. Εκτελεί μία εντολή
//   3. Επαναλαμβάνει
//
// Η κλήση συνάρτησης (CALL) PUSH-άρει νέο frame· η RETURN το POP-άρει.
// Η value stack (stack_) είναι κοινή μεταξύ όλων των frames.

void VM::run() {
    const Chunk& main = findChunk("__main__");
    callStack_.push_back({&main, 0, {}});

    while (!callStack_.empty()) {
        Frame& frame = callStack_.back();

        // Φυσικό τέλος frame (χωρίς RETURN) → void return
        if (frame.ip >= (int)frame.chunk->code.size()) {
            callStack_.pop_back();
            if (!callStack_.empty()) push(Value::Void());
            continue;
        }

        // Αντιγράφουμε την εντολή για να μην κρατάμε reference στο vector
        // (το CALL μπορεί να αλλάξει callStack_ και να κάνει re-allocate)
        const Instr instr = frame.chunk->code[frame.ip++];

        switch (instr.op) {

            case Op::PUSH_CONST:
                push(frame.chunk->constants[instr.a]);
                break;

            case Op::LOAD:
                push(loadVar(frame.chunk->names[instr.a]));
                break;

            case Op::STORE: {
                Value v = pop();
                storeVar(frame.chunk->names[instr.a], std::move(v));
                break;
            }

            case Op::ADD: case Op::SUB: case Op::MUL: case Op::DIV: case Op::MOD:
            case Op::EQ:  case Op::NEQ: case Op::LT:  case Op::GT:
            case Op::LEQ: case Op::GEQ: {
                Value b = pop(), a = pop();
                push(applyBinaryOp(instr.op, std::move(a), std::move(b)));
                break;
            }

            case Op::NEG: {
                Value a = pop();
                if (a.type == ValueType::Float) push(Value::Float(-a.num));
                else                            push(Value::Int(-(long long)a.num));
                break;
            }

            case Op::NOT:
                push(Value::Bool(!pop().truthy()));
                break;

            case Op::JUMP:
                // Ενημερώνει το ip του ΤΡΕΧΟΝΤΟΣ frame (όχι local `frame` που
                // μπορεί να έχει invalidated μετά από push/pop στο callStack_)
                callStack_.back().ip = instr.a;
                break;

            case Op::JUMP_IF_FALSE: {
                Value cond = pop();
                if (!cond.truthy()) callStack_.back().ip = instr.a;
                break;
            }

            case Op::CALL: {
                const std::string& fname = frame.chunk->names[instr.a];
                int argCount = instr.b;

                // ── Built-in: print ───────────────────────────────────────────
                if (fname == "print") {
                    std::vector<Value> args(argCount);
                    for (int i = argCount - 1; i >= 0; --i) args[i] = pop();
                    for (const auto& a : args) printValue(a);
                    push(Value::Void());
                    break;
                }

                // ── User-defined function ─────────────────────────────────────
                const Chunk& callee = findChunk(fname);

                // Pop arguments (were pushed left-to-right, so top = last arg)
                std::vector<Value> args(argCount);
                for (int i = argCount - 1; i >= 0; --i) args[i] = pop();

                Frame newFrame{&callee, 0, {}};
                for (int i = 0; i < (int)callee.params.size() && i < argCount; ++i)
                    newFrame.locals[callee.params[i]] = std::move(args[i]);

                callStack_.push_back(std::move(newFrame));
                break;
            }

            case Op::RETURN:
                callStack_.pop_back();
                if (!callStack_.empty()) push(Value::Void());
                continue;

            case Op::RETURN_VAL: {
                Value retVal = pop();
                callStack_.pop_back();
                push(std::move(retVal));
                continue;
            }

            case Op::ARRAY_NEW: {
                int count = instr.a;
                std::vector<Value> elems(count);
                for (int i = count - 1; i >= 0; --i) elems[i] = pop();
                push(Value::Array(std::move(elems)));
                break;
            }

            case Op::ARRAY_GET: {
                Value idx = pop();
                Value arr = pop();
                if (arr.type != ValueType::Array)
                    throw std::runtime_error("VM: cannot index a non-array value");
                int i = (int)idx.num;
                auto& elems = arr.array->elements;
                if (i < 0 || i >= (int)elems.size())
                    throw std::runtime_error("VM: index " + std::to_string(i) +
                                             " out of bounds (size " +
                                             std::to_string(elems.size()) + ")");
                push(elems[i]);
                break;
            }

            case Op::ARRAY_SET: {
                Value val = pop();
                Value idx = pop();
                Value arr = pop();
                if (arr.type != ValueType::Array)
                    throw std::runtime_error("VM: cannot index a non-array value");
                int i = (int)idx.num;
                auto& elems = arr.array->elements;
                if (i < 0 || i >= (int)elems.size())
                    throw std::runtime_error("VM: index " + std::to_string(i) +
                                             " out of bounds (size " +
                                             std::to_string(elems.size()) + ")");
                elems[i] = std::move(val);
                break;
            }

            case Op::POP:
                pop();
                break;

            case Op::DUP:
                push(peek());
                break;

            case Op::HALT:
                callStack_.clear();
                return;
        }
    }
}
