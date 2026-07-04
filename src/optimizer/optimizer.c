#include "optimizer.h"
#include "../index/btree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Helper function to count total rows in table
static uint32_t count_table_rows(Table* table) {
    uint32_t count = 0;
    Cursor* cursor = table_start(table);
    
    while (!cursor->end_of_table) {
        count++;
        cursor_advance(cursor);
    }
    
    free(cursor);
    return count;
}

QueryPlan* optimize_query(ParsedStatement* stmt, Table* table) {
    QueryPlan* plan = malloc(sizeof(QueryPlan));
    memset(plan, 0, sizeof(QueryPlan));
    
    // Get actual table size for better estimates
    uint32_t total_rows = count_table_rows(table);
    
    if (stmt->type == STMT_SELECT) {
        // Check if we can use index (B-tree search by ID)
        if (stmt->has_where && strcmp(stmt->where_clause->column, "id") == 0) {
            plan->scan_type = SCAN_INDEX_SEARCH;
            plan->index_column = strdup("id");
            plan->estimated_rows = 1;  // Expect to find 0 or 1 row
            
            // Cost: log(N) for tree traversal
            uint32_t tree_height = 1;
            uint32_t temp = total_rows;
            while (temp > LEAF_NODE_MAX_CELLS) {
                tree_height++;
                temp /= LEAF_NODE_MAX_CELLS;
            }
            plan->estimated_cost = tree_height * 5;
            plan->uses_index = true;
        } else {
            // Full table scan
            plan->scan_type = SCAN_FULL_TABLE;
            plan->index_column = NULL;
            plan->estimated_rows = total_rows;
            
            // Cost: O(N) - must scan every row
            plan->estimated_cost = total_rows > 0 ? total_rows * 5 : 1;
            plan->uses_index = false;
        }
    } else if (stmt->type == STMT_INSERT) {
        plan->scan_type = SCAN_INDEX_SEARCH;
        plan->index_column = strdup("id");
        plan->estimated_rows = 1;
        
        // Cost: log(N) to find position + write
        uint32_t tree_height = 1;
        uint32_t temp = total_rows;
        while (temp > LEAF_NODE_MAX_CELLS) {
            tree_height++;
            temp /= LEAF_NODE_MAX_CELLS;
        }
        plan->estimated_cost = tree_height * 5 + 10;
        plan->uses_index = true;
    } else if (stmt->type == STMT_UPDATE) {
        if (stmt->has_where && strcmp(stmt->where_clause->column, "id") == 0) {
            plan->scan_type = SCAN_INDEX_SEARCH;
            plan->index_column = strdup("id");
            plan->estimated_rows = 1;
            
            uint32_t tree_height = 1;
            uint32_t temp = total_rows;
            while (temp > LEAF_NODE_MAX_CELLS) {
                tree_height++;
                temp /= LEAF_NODE_MAX_CELLS;
            }
            plan->estimated_cost = tree_height * 5 + 15;
            plan->uses_index = true;
        } else {
            plan->scan_type = SCAN_FULL_TABLE;
            plan->index_column = NULL;
            plan->estimated_rows = total_rows;
            plan->estimated_cost = total_rows * 10 + 50;
            plan->uses_index = false;
        }
    } else if (stmt->type == STMT_DELETE) {
        if (stmt->has_where && strcmp(stmt->where_clause->column, "id") == 0) {
            plan->scan_type = SCAN_INDEX_SEARCH;
            plan->index_column = strdup("id");
            plan->estimated_rows = 1;
            
            uint32_t tree_height = 1;
            uint32_t temp = total_rows;
            while (temp > LEAF_NODE_MAX_CELLS) {
                tree_height++;
                temp /= LEAF_NODE_MAX_CELLS;
            }
            plan->estimated_cost = tree_height * 5 + 20;
            plan->uses_index = true;
        } else {
            plan->scan_type = SCAN_FULL_TABLE;
            plan->index_column = NULL;
            plan->estimated_rows = total_rows;
            plan->estimated_cost = total_rows * 10 + 100;
            plan->uses_index = false;
        }
    }
    
    return plan;
}

void print_query_plan(QueryPlan* plan) {
    printf("\n=== Query Plan ===\n");
    
    switch (plan->scan_type) {
        case SCAN_FULL_TABLE:
            printf("Scan Type: FULL TABLE SCAN\n");
            break;
        case SCAN_INDEX_SEARCH:
            printf("Scan Type: INDEX SEARCH (B+Tree)\n");
            break;
        case SCAN_INDEX_RANGE:
            printf("Scan Type: INDEX RANGE SCAN\n");
            break;
    }
    
    if (plan->index_column) {
        printf("Index Used: %s (Primary Key)\n", plan->index_column);
    } else {
        printf("Index Used: NONE (Sequential Scan)\n");
    }
    
    printf("Estimated Rows: %u\n", plan->estimated_rows);
    printf("Estimated Cost: %u", plan->estimated_cost);
    
    // Add interpretation
    if (plan->uses_index) {
        printf(" (O(log n) - Binary Search)\n");
    } else {
        printf(" (O(n) - Linear Scan)\n");
    }
    
    // Performance warning
    if (plan->scan_type == SCAN_FULL_TABLE && plan->estimated_rows > 100) {
        printf("\n⚠️  WARNING: Full table scan on large table!\n");
        printf("   Consider adding an index on the WHERE column.\n");
    }
    
    printf("==================\n\n");
}

void free_query_plan(QueryPlan* plan) {
    if (!plan) return;
    if (plan->index_column) {
        free(plan->index_column);
    }
    free(plan);
}

QueryStats* stats_create() {
    QueryStats* stats = malloc(sizeof(QueryStats));
    memset(stats, 0, sizeof(QueryStats));
    return stats;
}

void stats_update(QueryStats* stats, QueryPlan* plan, uint32_t rows_returned) {
    if (plan->scan_type == SCAN_FULL_TABLE) {
        stats->full_scans++;
    } else if (plan->scan_type == SCAN_INDEX_SEARCH) {
        stats->index_searches++;
    }
    
    stats->rows_scanned += plan->estimated_rows;
    stats->rows_returned += rows_returned;
}

void stats_print(QueryStats* stats) {
    printf("\n=== Query Statistics ===\n");
    printf("Full Table Scans: %u\n", stats->full_scans);
    printf("Index Searches: %u\n", stats->index_searches);
    printf("Total Rows Scanned: %u\n", stats->rows_scanned);
    printf("Total Rows Returned: %u\n", stats->rows_returned);
    
    if (stats->rows_scanned > 0) {
        float efficiency = (float)stats->rows_returned / stats->rows_scanned * 100;
        printf("Scan Efficiency: %.2f%%\n", efficiency);
    }
    
    printf("========================\n\n");
}

void stats_free(QueryStats* stats) {
    free(stats);
}
