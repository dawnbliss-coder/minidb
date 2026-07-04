#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "../storage/table.h"
#include "../storage/schema.h" 
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    STMT_SELECT,
    STMT_INSERT,
    STMT_UPDATE,
    STMT_DELETE,
    STMT_CREATE_TABLE,
    STMT_CREATE_INDEX
} StatementType;

typedef enum {
    AGG_NONE,
    AGG_COUNT,
    AGG_SUM,
    AGG_AVG,
    AGG_MAX,
    AGG_MIN
} AggregationType;

typedef struct {
    char* column;
    char* value;
} Assignment;

typedef struct {
    char* column;
    char* operator;
    char* value;
} Condition;

typedef struct {
    char left_table[64];
    char right_table[64];
    char left_column[32];
    char right_column[32];
} JoinClause;

typedef struct {
    StatementType type;
    Row row_to_insert;
    Assignment* assignments;
    int num_assignments;
    Condition* where_clause;
    bool has_where;
    bool is_explain;
    
    // For CREATE TABLE
    char table_name[64];
    ColumnDef* columns;
    uint32_t num_columns;

    // For CREATE INDEX  
    char index_table[64];
    char index_column[32];
    
    // For aggregations
    AggregationType agg_type;
    char agg_column[32];
    bool has_aggregation;
    
    // For ORDER BY and LIMIT
    char order_by_column[32];
    bool order_ascending;
    bool has_order_by;
    uint32_t limit;
    bool has_limit;
    
    // For JOIN  
    JoinClause* join_clause;
    bool has_join;
    char from_table[64];  // For multi-table support
} ParsedStatement;

// Function declarations
ParsedStatement* parse_statement(const char* input);
void free_parsed_statement(ParsedStatement* stmt);

#endif // PARSER_H
