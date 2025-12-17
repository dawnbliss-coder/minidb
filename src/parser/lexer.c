#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

Lexer* lexer_init(const char* input) {
    Lexer* lexer = malloc(sizeof(Lexer));
    lexer->input = input;
    lexer->position = 0;
    lexer->length = strlen(input);
    return lexer;
}

void lexer_free(Lexer* lexer) {
    free(lexer);
}

void token_free(Token* token) {
    if (token->value) {
        free(token->value);
    }
    free(token);
}

static void skip_whitespace(Lexer* lexer) {
    while (lexer->position < lexer->length && 
           isspace(lexer->input[lexer->position])) {
        lexer->position++;
    }
}

static Token* make_token(TokenType type, const char* value, int length) {
    Token* token = malloc(sizeof(Token));
    token->type = type;
    token->length = length;
    if (value) {
        token->value = malloc(length + 1);
        strncpy(token->value, value, length);
        token->value[length] = '\0';
    } else {
        token->value = NULL;
    }
    return token;
}

static Token* read_identifier(Lexer* lexer) {
    int start = lexer->position;
    
    while (lexer->position < lexer->length && 
           (isalnum(lexer->input[lexer->position]) || 
            lexer->input[lexer->position] == '_' ||
            lexer->input[lexer->position] == '@' ||
            lexer->input[lexer->position] == '.')) {
        lexer->position++;
    }
    
    int length = lexer->position - start;
    const char* value = &lexer->input[start];
    
    // Check for keywords (case-insensitive)
    if (strncasecmp(value, "count", length) == 0 && length == 5) {
        return make_token(TOKEN_COUNT, NULL, 0);
    } else if (strncasecmp(value, "sum", length) == 0 && length == 3) {
        return make_token(TOKEN_SUM, NULL, 0);
    } else if (strncasecmp(value, "avg", length) == 0 && length == 3) {
        return make_token(TOKEN_AVG, NULL, 0);
    } else if (strncasecmp(value, "max", length) == 0 && length == 3) {
        return make_token(TOKEN_MAX, NULL, 0);
    } else if (strncasecmp(value, "min", length) == 0 && length == 3) {
        return make_token(TOKEN_MIN, NULL, 0);
    } else if (strncasecmp(value, "order", length) == 0 && length == 5) {
        return make_token(TOKEN_ORDER, NULL, 0);
    } else if (strncasecmp(value, "by", length) == 0 && length == 2) {
        return make_token(TOKEN_BY, NULL, 0);
    } else if (strncasecmp(value, "limit", length) == 0 && length == 5) {
        return make_token(TOKEN_LIMIT, NULL, 0);
    } else if (strncasecmp(value, "asc", length) == 0 && length == 3) {
        return make_token(TOKEN_ASC, NULL, 0);
    } else if (strncasecmp(value, "desc", length) == 0 && length == 4) {
        return make_token(TOKEN_DESC, NULL, 0);
    } else if (strncasecmp(value, "index", length) == 0 && length == 5) {
        return make_token(TOKEN_INDEX, NULL, 0);
    } else if (strncasecmp(value, "create", length) == 0 && length == 6) {
        return make_token(TOKEN_CREATE, NULL, 0);
    } else if (strncasecmp(value, "table", length) == 0 && length == 5) {
        return make_token(TOKEN_TABLE, NULL, 0);
    } else if (strncasecmp(value, "join", length) == 0 && length == 4) {
        return make_token(TOKEN_JOIN, NULL, 0);
    } else if (strncasecmp(value, "inner", length) == 0 && length == 5) {
        return make_token(TOKEN_INNER, NULL, 0);
    } else if (strncasecmp(value, "on", length) == 0 && length == 2) {
        return make_token(TOKEN_ON, NULL, 0);
    } else if (strncasecmp(value, "int", length) == 0 && length == 3) {
        return make_token(TOKEN_INT, NULL, 0);
    } else if (strncasecmp(value, "varchar", length) == 0 && length == 7) {
        return make_token(TOKEN_VARCHAR, NULL, 0);
    } else if (strncasecmp(value, "primary", length) == 0 && length == 7) {
        return make_token(TOKEN_PRIMARY, NULL, 0);
    } else if (strncasecmp(value, "key", length) == 0 && length == 3) {
        return make_token(TOKEN_KEY, NULL, 0);
    } else if (strncasecmp(value, "explain", length) == 0 && length == 7) {
        return make_token(TOKEN_EXPLAIN, NULL, 0);
    } else if (strncasecmp(value, "select", length) == 0 && length == 6) {
        return make_token(TOKEN_SELECT, NULL, 0);
    } else if (strncasecmp(value, "insert", length) == 0 && length == 6) {
        return make_token(TOKEN_INSERT, NULL, 0);
    } else if (strncasecmp(value, "update", length) == 0 && length == 6) {
        return make_token(TOKEN_UPDATE, NULL, 0);
    } else if (strncasecmp(value, "delete", length) == 0 && length == 6) {
        return make_token(TOKEN_DELETE, NULL, 0);
    } else if (strncasecmp(value, "set", length) == 0 && length == 3) {
        return make_token(TOKEN_SET, NULL, 0);
    } else if (strncasecmp(value, "where", length) == 0 && length == 5) {
        return make_token(TOKEN_WHERE, NULL, 0);
    } else if (strncasecmp(value, "from", length) == 0 && length == 4) {
        return make_token(TOKEN_FROM, NULL, 0);
    } else if (strncasecmp(value, "into", length) == 0 && length == 4) {
        return make_token(TOKEN_INTO, NULL, 0);
    } else if (strncasecmp(value, "values", length) == 0 && length == 6) {
        return make_token(TOKEN_VALUES, NULL, 0);
    } else {
        return make_token(TOKEN_IDENTIFIER, value, length);
    }
}

static Token* read_number(Lexer* lexer) {
    int start = lexer->position;
    
    while (lexer->position < lexer->length && 
           isdigit(lexer->input[lexer->position])) {
        lexer->position++;
    }
    
    int length = lexer->position - start;
    return make_token(TOKEN_NUMBER, &lexer->input[start], length);
}

static Token* read_string(Lexer* lexer) {
    lexer->position++; // Skip opening quote
    int start = lexer->position;
    
    while (lexer->position < lexer->length && 
           lexer->input[lexer->position] != '\'') {
        lexer->position++;
    }
    
    int length = lexer->position - start;
    lexer->position++; // Skip closing quote
    
    return make_token(TOKEN_STRING, &lexer->input[start], length);
}

Token* lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);
    
    if (lexer->position >= lexer->length) {
        return make_token(TOKEN_EOF, NULL, 0);
    }
    
    char current = lexer->input[lexer->position];
    
    // Single character tokens
    switch (current) {
        case '=':
            lexer->position++;
            return make_token(TOKEN_EQUALS, NULL, 0);
        case ',':
            lexer->position++;
            return make_token(TOKEN_COMMA, NULL, 0);
        case '*':
            lexer->position++;
            return make_token(TOKEN_ASTERISK, NULL, 0);
        case '(':
            lexer->position++;
            return make_token(TOKEN_LPAREN, NULL, 0);
        case ')':
            lexer->position++;
            return make_token(TOKEN_RPAREN, NULL, 0);
        case ';':
            lexer->position++;
            return make_token(TOKEN_SEMICOLON, NULL, 0);
        case '\'':
            return read_string(lexer);
    }
    
    // Multi-character tokens
    if (isalpha(current) || current == '_') {
        return read_identifier(lexer);
    }
    
    if (isdigit(current)) {
        return read_number(lexer);
    }
    
    // Unknown character
    lexer->position++;
    return make_token(TOKEN_ERROR, &current, 1);
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EXPLAIN: return "EXPLAIN"; 
        case TOKEN_SELECT: return "SELECT";
        case TOKEN_INSERT: return "INSERT";
        case TOKEN_UPDATE: return "UPDATE";
        case TOKEN_DELETE: return "DELETE";
        case TOKEN_SET: return "SET";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_FROM: return "FROM";
        case TOKEN_INTO: return "INTO";
        case TOKEN_VALUES: return "VALUES";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_ASTERISK: return "ASTERISK";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
