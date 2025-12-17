#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <stdint.h>
#include <stdbool.h>
#include "../parser/parser.h"
#include "../storage/table.h"

typedef enum {
    SCAN_FULL_TABLE,
    SCAN_INDEX_SEARCH,
    SCAN_INDEX_RANGE
} ScanType;

typedef struct {
    ScanType scan_type;
    char* index_column;
    uint32_t estimated_rows;
    uint32_t estimated_cost;
    bool uses_index;
} QueryPlan;

typedef struct {
    uint32_t full_scans;
    uint32_t index_searches;
    uint32_t rows_scanned;
    uint32_t rows_returned;
} QueryStats;

// Function declarations
QueryPlan* optimize_query(ParsedStatement* stmt, Table* table);
void print_query_plan(QueryPlan* plan);
void free_query_plan(QueryPlan* plan);
QueryStats* stats_create();
void stats_update(QueryStats* stats, QueryPlan* plan, uint32_t rows_returned);
void stats_print(QueryStats* stats);
void stats_free(QueryStats* stats);

#endif // OPTIMIZER_H
