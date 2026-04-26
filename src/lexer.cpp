#include "lexer.h"
#include <unordered_map>
#include <stdexcept>

// Χαρτογράφηση λέξεων → keyword tokens
static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"let",    TokenType::LET},
    {"fn",     TokenType::FN},
    {"return", TokenType::RETURN},
    {"if",     TokenType::IF},
    {"else",   TokenType::ELSE},
    {"while",  TokenType::WHILE},
    {"for",    TokenType::FOR},
    {"true",   TokenType::TRUE_KW},
    {"false",  TokenType::FALSE_KW},
};

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::NUMBER:     return "NUMBER";
        case TokenType::STRING:     return "STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::LET:        return "LET";
        case TokenType::FN:         return "FN";
        case TokenType::RETURN:     return "RETURN";
        case TokenType::IF:         return "IF";
        case TokenType::ELSE:       return "ELSE";
        case TokenType::WHILE:      return "WHILE";
        case TokenType::FOR:        return "FOR";
        case TokenType::TRUE_KW:    return "TRUE";
        case TokenType::FALSE_KW:   return "FALSE";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::STAR:       return "STAR";
        case TokenType::SLASH:      return "SLASH";
        case TokenType::PERCENT:    return "PERCENT";
        case TokenType::EQUALS:     return "EQUALS";
        case TokenType::EQEQ:       return "EQEQ";
        case TokenType::BANG:       return "BANG";
        case TokenType::NEQ:        return "NEQ";
        case TokenType::LT:         return "LT";
        case TokenType::GT:         return "GT";
        case TokenType::LEQ:        return "LEQ";
        case TokenType::GEQ:        return "GEQ";
        case TokenType::LPAREN:     return "LPAREN";
        case TokenType::RPAREN:     return "RPAREN";
        case TokenType::LBRACE:     return "LBRACE";
        case TokenType::RBRACE:     return "RBRACE";
        case TokenType::COLON:      return "COLON";
        case TokenType::ARROW:      return "ARROW";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::SEMICOLON:  return "SEMICOLON";
        case TokenType::DOT:        return "DOT";
        case TokenType::EOF_TOKEN:  return "EOF";
        case TokenType::UNKNOWN:    return "UNKNOWN";
        default:                    return "?";
    }
}

// ── Constructor ────────────────────────────────────────────────────────────────

Lexer::Lexer(const std::string& source) : source(source) {}

// ── Βοηθητικές ────────────────────────────────────────────────────────────────

// Επιστρέφει τον τρέχοντα χαρακτήρα (ή '\0' αν φτάσαμε στο τέλος)
char Lexer::current() const {
    return pos < source.size() ? source[pos] : '\0';
}

// Κοιτάζει μπροστά χωρίς να προχωράει
char Lexer::peek(int offset) const {
    size_t p = pos + offset;
    return p < source.size() ? source[p] : '\0';
}

// Προχωράει έναν χαρακτήρα και ενημερώνει line/col
void Lexer::advance() {
    if (current() == '\n') { line++; col = 1; }
    else                   { col++; }
    pos++;
}

// Παρακάμπτει κενά, tabs, newlines και σχόλια (//)
void Lexer::skipWhitespaceAndComments() {
    while (pos < source.size()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek() == '/') {
            // Σχόλιο μέχρι τέλος γραμμής
            while (pos < source.size() && current() != '\n')
                advance();
        } else {
            break;
        }
    }
}

// ── Readers ───────────────────────────────────────────────────────────────────

Token Lexer::readNumber() {
    Token tok;
    tok.line = line;
    tok.col  = col;
    tok.type = TokenType::NUMBER;

    while (pos < source.size() && isdigit(current()))
        tok.value += current(), advance();

    // Δεκαδικός αριθμός (π.χ. 3.14)
    if (current() == '.' && isdigit(peek())) {
        tok.value += current(); advance();
        while (pos < source.size() && isdigit(current()))
            tok.value += current(), advance();
    }

    return tok;
}

Token Lexer::readString() {
    Token tok;
    tok.line = line;
    tok.col  = col;
    tok.type = TokenType::STRING;

    advance(); // παράλειψε το άνοιγμα "
    while (pos < source.size() && current() != '"') {
        if (current() == '\\' && peek() == '"') {
            tok.value += '"'; advance(); advance(); // escaped quote
        } else {
            tok.value += current(); advance();
        }
    }
    if (current() == '"') advance(); // παράλειψε το κλείσιμο "
    else throw std::runtime_error("Unterminated string at line " + std::to_string(line));

    return tok;
}

Token Lexer::readIdentifierOrKeyword() {
    Token tok;
    tok.line = line;
    tok.col  = col;

    while (pos < source.size() && (isalnum(current()) || current() == '_'))
        tok.value += current(), advance();

    // Είναι keyword ή identifier;
    auto it = KEYWORDS.find(tok.value);
    tok.type = (it != KEYWORDS.end()) ? it->second : TokenType::IDENTIFIER;
    return tok;
}

// ── Κύρια συνάρτηση ───────────────────────────────────────────────────────────

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (pos < source.size()) {
        skipWhitespaceAndComments();
        if (pos >= source.size()) break;

        char c = current();

        if (isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }
        if (isalpha(c) || c == '_') {
            tokens.push_back(readIdentifierOrKeyword());
            continue;
        }

        // Operators & σύμβολα
        Token tok;
        tok.line = line;
        tok.col  = col;

        switch (c) {
            case '+': tok.type = TokenType::PLUS;      tok.value = "+";  break;
            case '*': tok.type = TokenType::STAR;      tok.value = "*";  break;
            case '/': tok.type = TokenType::SLASH;     tok.value = "/";  break;
            case '%': tok.type = TokenType::PERCENT;   tok.value = "%";  break;
            case '(': tok.type = TokenType::LPAREN;    tok.value = "(";  break;
            case ')': tok.type = TokenType::RPAREN;    tok.value = ")";  break;
            case '{': tok.type = TokenType::LBRACE;    tok.value = "{";  break;
            case '}': tok.type = TokenType::RBRACE;    tok.value = "}";  break;
            case ':': tok.type = TokenType::COLON;     tok.value = ":";  break;
            case ',': tok.type = TokenType::COMMA;     tok.value = ",";  break;
            case ';': tok.type = TokenType::SEMICOLON; tok.value = ";";  break;
            case '.': tok.type = TokenType::DOT;       tok.value = ".";  break;

            case '-':
                if (peek() == '>') {
                    tok.type = TokenType::ARROW; tok.value = "->"; advance();
                } else {
                    tok.type = TokenType::MINUS; tok.value = "-";
                }
                break;
            case '=':
                if (peek() == '=') {
                    tok.type = TokenType::EQEQ; tok.value = "=="; advance();
                } else {
                    tok.type = TokenType::EQUALS; tok.value = "=";
                }
                break;
            case '!':
                if (peek() == '=') {
                    tok.type = TokenType::NEQ; tok.value = "!="; advance();
                } else {
                    tok.type = TokenType::BANG; tok.value = "!";
                }
                break;
            case '<':
                if (peek() == '=') {
                    tok.type = TokenType::LEQ; tok.value = "<="; advance();
                } else {
                    tok.type = TokenType::LT; tok.value = "<";
                }
                break;
            case '>':
                if (peek() == '=') {
                    tok.type = TokenType::GEQ; tok.value = ">="; advance();
                } else {
                    tok.type = TokenType::GT; tok.value = ">";
                }
                break;

            default:
                tok.type  = TokenType::UNKNOWN;
                tok.value = std::string(1, c);
        }

        advance();
        tokens.push_back(tok);
    }

    tokens.push_back({TokenType::EOF_TOKEN, "", line, col});
    return tokens;
}
