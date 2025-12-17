#include "parser.h"
#include "../storage/schema.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    Lexer* lexer;
    Token* current_token;
} Parser;

static void parser_advance(Parser* parser) {
    if (parser->current_token) {
        token_free(parser->current_token);
    }
    parser->current_token = lexer_next_token(parser->lexer);
}

static bool parser_expect(Parser* parser, TokenType type) {  // <-- MOVE THIS UP HERE
    if (parser->current_token->type == type) {
        parser_advance(parser);
        return true;
    }
    return false;
}

static Condition* parse_where_clause(Parser* parser) {
    if (parser->current_token->type != TOKEN_WHERE) {
        return NULL;
    }
    parser_advance(parser); // Skip WHERE
    
    if (parser->current_token->type != TOKEN_IDENTIFIER) {
        return NULL;
    }
    
    Condition* condition = malloc(sizeof(Condition));
    condition->column = strdup(parser->current_token->value);
    parser_advance(parser);
    
    if (parser->current_token->type != TOKEN_EQUALS) {
        free(condition->column);
        free(condition);
        return NULL;
    }
    condition->operator = strdup("=");
    parser_advance(parser);
    
    // Accept NUMBER, STRING, or IDENTIFIER for value
    if (parser->current_token->type == TOKEN_NUMBER || 
        parser->current_token->type == TOKEN_STRING ||
        parser->current_token->type == TOKEN_IDENTIFIER) {
        condition->value = strdup(parser->current_token->value);
        parser_advance(parser);
    } else {
        free(condition->column);
        free(condition->operator);
        free(condition);
        return NULL;
    }
    
    return condition;
}

static ParsedStatement* parse_create_table(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_CREATE_TABLE;
    
    parser_advance(parser); // Skip CREATE
    
    if (!parser_expect(parser, TOKEN_TABLE)) {
        free(stmt);
        return NULL;
    }
    
    // Get table name
    if (parser->current_token->type != TOKEN_IDENTIFIER) {
        free(stmt);
        return NULL;
    }
    strncpy(stmt->table_name, parser->current_token->value, 63);
    parser_advance(parser);
    
    if (!parser_expect(parser, TOKEN_LPAREN)) {
        free(stmt);
        return NULL;
    }
    
    // Parse columns
    stmt->columns = malloc(sizeof(ColumnDef) * MAX_COLUMNS);
    stmt->num_columns = 0;
    
    while (parser->current_token->type != TOKEN_RPAREN && 
           parser->current_token->type != TOKEN_EOF) {
        
        if (stmt->num_columns >= MAX_COLUMNS) {
            break;
        }
        
        ColumnDef* col = &stmt->columns[stmt->num_columns];
        memset(col, 0, sizeof(ColumnDef));
        
        // Column name
        if (parser->current_token->type != TOKEN_IDENTIFIER) {
            break;
        }
        strncpy(col->name, parser->current_token->value, MAX_COLUMN_NAME - 1);
        parser_advance(parser);
        
        // Column type
        if (parser->current_token->type == TOKEN_INT) {
            col->type = TYPE_INT;
            col->size = 4;
            parser_advance(parser);
        } else if (parser->current_token->type == TOKEN_VARCHAR) {
            col->type = TYPE_VARCHAR;
            parser_advance(parser);
            
            if (parser_expect(parser, TOKEN_LPAREN)) {
                if (parser->current_token->type == TOKEN_NUMBER) {
                    col->size = atoi(parser->current_token->value);
                    parser_advance(parser);
                }
                parser_expect(parser, TOKEN_RPAREN);
            }
        }
        
        // Check for PRIMARY KEY
        if (parser->current_token->type == TOKEN_PRIMARY) {
            parser_advance(parser);
            if (parser_expect(parser, TOKEN_KEY)) {
                col->is_primary_key = true;
            }
        }
        
        stmt->num_columns++;
        
        if (parser->current_token->type == TOKEN_COMMA) {
            parser_advance(parser);
        }
    }
    
    parser_expect(parser, TOKEN_RPAREN);
    
    return stmt;
}

static ParsedStatement* parse_create_index(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_CREATE_INDEX;
    
    parser_advance(parser); // Skip CREATE
    
    if (!parser_expect(parser, TOKEN_INDEX)) {
        free(stmt);
        return NULL;
    }
    
    // ON table_name (column_name)
    if (parser_expect(parser, TOKEN_ON)) {
        if (parser->current_token->type == TOKEN_IDENTIFIER) {
            strncpy(stmt->index_table, parser->current_token->value, 63);
            parser_advance(parser);
        }
        
        if (parser_expect(parser, TOKEN_LPAREN)) {
            if (parser->current_token->type == TOKEN_IDENTIFIER) {
                strncpy(stmt->index_column, parser->current_token->value, 31);
                parser_advance(parser);
            }
            parser_expect(parser, TOKEN_RPAREN);
        }
    }
    
    return stmt;
}

static ParsedStatement* parse_select(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_SELECT;
    stmt->agg_type = AGG_NONE;
    stmt->has_aggregation = false;
    stmt->order_ascending = true;
    stmt->has_order_by = false;
    stmt->has_limit = false;
    stmt->has_join = false;
    
    parser_advance(parser); // Skip SELECT
    
    // Check for aggregation functions (keep existing code)
    if (parser->current_token->type == TOKEN_COUNT) {
        stmt->agg_type = AGG_COUNT;
        stmt->has_aggregation = true;
        parser_advance(parser);
        
        if (parser_expect(parser, TOKEN_LPAREN)) {
            if (parser->current_token->type == TOKEN_ASTERISK) {
                strcpy(stmt->agg_column, "*");
                parser_advance(parser);
            } else if (parser->current_token->type == TOKEN_IDENTIFIER) {
                strncpy(stmt->agg_column, parser->current_token->value, 31);
                parser_advance(parser);
            }
            parser_expect(parser, TOKEN_RPAREN);
        }
    } else if (parser->current_token->type == TOKEN_ASTERISK) {
        parser_advance(parser);
    } else {
        free(stmt);
        return NULL;
    }
    
    // Optional FROM clause for table name
    if (parser->current_token->type == TOKEN_FROM) {
        parser_advance(parser);
        if (parser->current_token->type == TOKEN_IDENTIFIER) {
            strncpy(stmt->from_table, parser->current_token->value, 63);
            parser_advance(parser);
        }
        
        // Check for JOIN
        if (parser->current_token->type == TOKEN_INNER || 
            parser->current_token->type == TOKEN_JOIN) {
            
            if (parser->current_token->type == TOKEN_INNER) {
                parser_advance(parser);
            }
            
            if (parser_expect(parser, TOKEN_JOIN)) {
                stmt->join_clause = malloc(sizeof(JoinClause));
                strncpy(stmt->join_clause->left_table, stmt->from_table, 63);
                
                // Get right table name
                if (parser->current_token->type == TOKEN_IDENTIFIER) {
                    strncpy(stmt->join_clause->right_table, parser->current_token->value, 63);
                    parser_advance(parser);
                }
                
                // ON clause
                if (parser_expect(parser, TOKEN_ON)) {
                    // Parse left_table.column = right_table.column
                    if (parser->current_token->type == TOKEN_IDENTIFIER) {
                        // This should be table.column format
                        char* dot = strchr(parser->current_token->value, '.');
                        if (dot) {
                            int table_len = dot - parser->current_token->value;
                            strncpy(stmt->join_clause->left_table, parser->current_token->value, table_len);
                            stmt->join_clause->left_table[table_len] = '\0';
                            strncpy(stmt->join_clause->left_column, dot + 1, 31);
                        }
                        parser_advance(parser);
                    }
                    
                    parser_expect(parser, TOKEN_EQUALS);
                    
                    if (parser->current_token->type == TOKEN_IDENTIFIER) {
                        char* dot = strchr(parser->current_token->value, '.');
                        if (dot) {
                            int table_len = dot - parser->current_token->value;
                            strncpy(stmt->join_clause->right_table, parser->current_token->value, table_len);
                            stmt->join_clause->right_table[table_len] = '\0';
                            strncpy(stmt->join_clause->right_column, dot + 1, 31);
                        }
                        parser_advance(parser);
                    }
                }
                
                stmt->has_join = true;
            }
        }
    }
    
    // Optional WHERE clause (keep existing code)
    stmt->where_clause = parse_where_clause(parser);
    stmt->has_where = (stmt->where_clause != NULL);
    
    // Optional ORDER BY (keep existing code)
    if (parser->current_token->type == TOKEN_ORDER) {
        parser_advance(parser);
        if (parser_expect(parser, TOKEN_BY)) {
            if (parser->current_token->type == TOKEN_IDENTIFIER) {
                strncpy(stmt->order_by_column, parser->current_token->value, 31);
                stmt->has_order_by = true;
                parser_advance(parser);
                
                if (parser->current_token->type == TOKEN_ASC) {
                    stmt->order_ascending = true;
                    parser_advance(parser);
                } else if (parser->current_token->type == TOKEN_DESC) {
                    stmt->order_ascending = false;
                    parser_advance(parser);
                }
            }
        }
    }
    
    // Optional LIMIT (keep existing code)
    if (parser->current_token->type == TOKEN_LIMIT) {
        parser_advance(parser);
        if (parser->current_token->type == TOKEN_NUMBER) {
            stmt->limit = atoi(parser->current_token->value);
            stmt->has_limit = true;
            parser_advance(parser);
        }
    }
    
    return stmt;
}

static ParsedStatement* parse_insert(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_INSERT;
    
    parser_advance(parser); // Skip INSERT
    
    // Parse: id username email
    if (parser->current_token->type != TOKEN_NUMBER) {
        free(stmt);
        return NULL;
    }
    stmt->row_to_insert.id = atoi(parser->current_token->value);
    parser_advance(parser);
    
    if (parser->current_token->type != TOKEN_IDENTIFIER && 
        parser->current_token->type != TOKEN_STRING) {
        free(stmt);
        return NULL;
    }
    strncpy(stmt->row_to_insert.username, parser->current_token->value, 
            COLUMN_USERNAME_SIZE);
    parser_advance(parser);
    
    if (parser->current_token->type != TOKEN_IDENTIFIER && 
        parser->current_token->type != TOKEN_STRING) {
        free(stmt);
        return NULL;
    }
    strncpy(stmt->row_to_insert.email, parser->current_token->value, 
            COLUMN_EMAIL_SIZE);
    parser_advance(parser);
    
    return stmt;
}

static ParsedStatement* parse_update(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_UPDATE;
    
    parser_advance(parser); // Skip UPDATE
    
    if (!parser_expect(parser, TOKEN_SET)) {
        free(stmt);
        return NULL;
    }
    
    // Parse: column = value
    if (parser->current_token->type != TOKEN_IDENTIFIER) {
        free(stmt);
        return NULL;
    }
    
    stmt->num_assignments = 1;
    stmt->assignments = malloc(sizeof(Assignment));
    stmt->assignments[0].column = strdup(parser->current_token->value);
    parser_advance(parser);
    
    if (!parser_expect(parser, TOKEN_EQUALS)) {
        free(stmt->assignments[0].column);
        free(stmt->assignments);
        free(stmt);
        return NULL;
    }
    
    // Accept NUMBER, STRING, or IDENTIFIER for value
    if (parser->current_token->type == TOKEN_NUMBER || 
        parser->current_token->type == TOKEN_STRING ||
        parser->current_token->type == TOKEN_IDENTIFIER) {
        stmt->assignments[0].value = strdup(parser->current_token->value);
        parser_advance(parser);
    } else {
        free(stmt->assignments[0].column);
        free(stmt->assignments);
        free(stmt);
        return NULL;
    }
    
    // WHERE clause
    stmt->where_clause = parse_where_clause(parser);
    stmt->has_where = (stmt->where_clause != NULL);
    
    return stmt;
}

static ParsedStatement* parse_delete(Parser* parser) {
    ParsedStatement* stmt = malloc(sizeof(ParsedStatement));
    memset(stmt, 0, sizeof(ParsedStatement));
    stmt->type = STMT_DELETE;
    
    parser_advance(parser); // Skip DELETE
    
    // WHERE clause
    stmt->where_clause = parse_where_clause(parser);
    stmt->has_where = (stmt->where_clause != NULL);
    
    return stmt;
}

ParsedStatement* parse_statement(const char* input) {
    Parser parser;
    parser.lexer = lexer_init(input);
    parser.current_token = NULL;
    parser_advance(&parser);
    
    ParsedStatement* stmt = NULL;
    bool is_explain = false;
    
    if (parser.current_token->type == TOKEN_EXPLAIN) {
        is_explain = true;
        parser_advance(&parser);
    }
    
    switch (parser.current_token->type) {
        case TOKEN_CREATE:
            // Look ahead to see if it's TABLE or INDEX
            parser_advance(&parser);
            if (parser.current_token->type == TOKEN_TABLE) {
                // Go back and parse from CREATE
                token_free(parser.current_token);
                lexer_free(parser.lexer);
                
                // Restart parser
                parser.lexer = lexer_init(input);
                parser.current_token = NULL;
                if (is_explain) parser_advance(&parser); // Skip EXPLAIN if present
                parser_advance(&parser); // Now at CREATE
                stmt = parse_create_table(&parser);
            } else if (parser.current_token->type == TOKEN_INDEX) {
                // Go back and parse from CREATE
                token_free(parser.current_token);
                lexer_free(parser.lexer);
                
                // Restart parser
                parser.lexer = lexer_init(input);
                parser.current_token = NULL;
                if (is_explain) parser_advance(&parser); // Skip EXPLAIN if present
                parser_advance(&parser); // Now at CREATE
                stmt = parse_create_index(&parser);
            }
            break;
        case TOKEN_SELECT:
            stmt = parse_select(&parser);
            break;
        case TOKEN_INSERT:
            stmt = parse_insert(&parser);
            break;
        case TOKEN_UPDATE:
            stmt = parse_update(&parser);
            break;
        case TOKEN_DELETE:
            stmt = parse_delete(&parser);
            break;
        default:
            break;
    }
    
    if (stmt) {
        stmt->is_explain = is_explain;
    }
    
    token_free(parser.current_token);
    lexer_free(parser.lexer);
    
    return stmt;
}

void free_parsed_statement(ParsedStatement* stmt) {
    if (!stmt) return;
    
    if (stmt->where_clause) {
        free(stmt->where_clause->column);
        free(stmt->where_clause->operator);
        free(stmt->where_clause->value);
        free(stmt->where_clause);
    }
    
    if (stmt->assignments) {
        for (int i = 0; i < stmt->num_assignments; i++) {
            free(stmt->assignments[i].column);
            free(stmt->assignments[i].value);
        }
        free(stmt->assignments);
    }
    
    if (stmt->columns) {
        free(stmt->columns);
    }
    
    if (stmt->join_clause) {  // <-- ADD
        free(stmt->join_clause);
    }
    
    free(stmt);
}

