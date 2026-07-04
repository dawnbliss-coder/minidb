#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOKEN_SELECT,
    TOKEN_INSERT,
    TOKEN_UPDATE,
    TOKEN_DELETE,
    TOKEN_EXPLAIN,
    TOKEN_CREATE,
    TOKEN_TABLE,
    TOKEN_INDEX,
    TOKEN_JOIN,
    TOKEN_INNER,
    TOKEN_ON,
    TOKEN_INT,
    TOKEN_VARCHAR,
    TOKEN_PRIMARY,
    TOKEN_KEY,
    TOKEN_COUNT,       // <-- ADD
    TOKEN_SUM,         // <-- ADD
    TOKEN_AVG,         // <-- ADD
    TOKEN_MAX,         // <-- ADD
    TOKEN_MIN,         // <-- ADD
    TOKEN_ORDER,       // <-- ADD
    TOKEN_BY,          // <-- ADD
    TOKEN_LIMIT,       // <-- ADD
    TOKEN_ASC,         // <-- ADD
    TOKEN_DESC,        // <-- ADD
    TOKEN_SET,
    TOKEN_WHERE,
    TOKEN_FROM,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_EQUALS,
    TOKEN_COMMA,
    TOKEN_ASTERISK,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int length;
} Token;

typedef struct {
    const char* input;
    int position;
    int length;
} Lexer;

// Function declarations
Lexer* lexer_init(const char* input);
void lexer_free(Lexer* lexer);
Token* lexer_next_token(Lexer* lexer);
void token_free(Token* token);
const char* token_type_to_string(TokenType type);

#endif // LEXER_H
