#pragma once
#include <string>
#include <vector>

// Κάθε "λέξη" του κώδικα έχει έναν τύπο
enum class TokenType {
    // Literals — τιμές που γράφει ο προγραμματιστής
    NUMBER,       // 42, 3.14
    STRING,       // "hello"
    IDENTIFIER,   // x, myVar, add

    // Keywords — δεσμευμένες λέξεις της γλώσσας
    LET,          // let
    FN,           // fn
    RETURN,       // return
    IF,           // if
    ELSE,         // else
    WHILE,        // while
    FOR,          // for
    TRUE_KW,      // true
    FALSE_KW,     // false

    // Operators — πράξεις
    PLUS,         // +
    MINUS,        // -
    STAR,         // *
    SLASH,        // /
    PERCENT,      // %
    EQUALS,       // =
    EQEQ,         // ==
    BANG,         // !
    NEQ,          // !=
    LT,           // <
    GT,           // >
    LEQ,          // <=
    GEQ,          // >=

    // Σύμβολα
    LPAREN,       // (
    RPAREN,       // )
    LBRACE,       // {
    RBRACE,       // }
    COLON,        // :
    ARROW,        // ->
    COMMA,        // ,
    SEMICOLON,    // ;
    DOT,          // .

    // Ειδικά
    EOF_TOKEN,    // τέλος αρχείου
    UNKNOWN       // άγνωστος χαρακτήρας
};

// Μετατρέπει TokenType σε string για debugging
std::string tokenTypeToString(TokenType type);

// Ένα Token = ένα κομμάτι κώδικα
struct Token {
    TokenType   type;   // τι είναι
    std::string value;  // η πραγματική τιμή (π.χ. "42", "+", "myVar")
    int         line;   // σε ποια γραμμή βρέθηκε
    int         col;    // σε ποια στήλη βρέθηκε
};

// Ο Lexer: παίρνει source code (string) και επιστρέφει λίστα από Tokens
class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t pos  = 0;
    int    line = 1;
    int    col  = 1;

    char current() const;
    char peek(int offset = 1) const;
    void advance();
    void skipWhitespaceAndComments();

    Token readNumber();
    Token readString();
    Token readIdentifierOrKeyword();
};
