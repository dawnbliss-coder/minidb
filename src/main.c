#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "storage/table.h"
#include "index/btree.h"
#include "index/secondary_index.h" 
#include "parser/parser.h"
#include "optimizer/optimizer.h"
#include "storage/schema.h"
#include "storage/table_manager.h"

#define INPUT_BUFFER_SIZE 1024
static QueryStats* global_stats = NULL;
static Schema* global_schema = NULL;
static TableManager* table_manager = NULL;
static IndexManager* index_manager = NULL;

typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED
} MetaCommandResult;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_TABLE_FULL,
    EXECUTE_NOT_FOUND
} ExecuteResult;

InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}

void print_prompt() {
    printf("minidb> ");
}

void read_input(InputBuffer* input_buffer) {
    ssize_t bytes_read = getline(&(input_buffer->buffer), 
                                  &(input_buffer->buffer_length), 
                                  stdin);
    
    if (bytes_read <= 0) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }
    
    // Remove trailing newline
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        if (global_stats) {
            stats_free(global_stats);
        }
        if (global_schema) {
            schema_free(global_schema);
        }
        if (table_manager) {  
            table_manager_free(table_manager);
        }
        table_close(table);
        close_input_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    } else if (strcmp(input_buffer->buffer, ".indexes") == 0) {  // <-- ADD
        if (index_manager) {
            printf("\n=== Secondary Indexes ===\n");
            for (uint32_t i = 0; i < index_manager->num_indexes; i++) {
                secondary_index_print(&index_manager->indexes[i]);
            }
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".schema") == 0) {  // <-- ADD
        if (global_schema) {
            schema_print(global_schema);
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".stats") == 0) {  // <-- ADD THIS
        if (global_stats) {
            stats_print(global_stats);
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".checkpoint") == 0) {
        if (table->wal) {
            wal_checkpoint(table->wal, table->pager);
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".begin") == 0) {
        if (table->wal) {
            wal_begin_transaction(table->wal);
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".commit") == 0) {
        if (table->wal) {
            wal_commit_transaction(table->wal);
        }
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
        printf("Constants:\n");
        printf("ROW_SIZE: %lu\n", ROW_SIZE);
        printf("COMMON_NODE_HEADER_SIZE: %lu\n", COMMON_NODE_HEADER_SIZE);
        printf("LEAF_NODE_HEADER_SIZE: %lu\n", LEAF_NODE_HEADER_SIZE);
        printf("LEAF_NODE_CELL_SIZE: %lu\n", LEAF_NODE_CELL_SIZE);
        printf("LEAF_NODE_SPACE_FOR_CELLS: %lu\n", LEAF_NODE_SPACE_FOR_CELLS);
        printf("LEAF_NODE_MAX_CELLS: %lu\n", LEAF_NODE_MAX_CELLS);
        return META_COMMAND_SUCCESS;
    } else {
        return META_COMMAND_UNRECOGNIZED;
    }
}

ExecuteResult execute_create_index(ParsedStatement* stmt, Table* table) {
    if (!index_manager) {
        index_manager = index_manager_create();
    }
    
    if (index_manager_create_index(index_manager, stmt->index_table, stmt->index_column)) {
        // Build the index from existing table data
        index_manager_build_from_table(index_manager, stmt->index_table, stmt->index_column, table);
        return EXECUTE_SUCCESS;
    }
    
    return EXECUTE_TABLE_FULL;
}

ExecuteResult execute_insert(ParsedStatement* stmt, Table* table) {
    void* node = pager_get_page(table->pager, table->root_page_num);
    uint32_t num_cells = (*leaf_node_num_cells(node));
    
    Row* row_to_insert = &(stmt->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;
    Cursor* cursor = table_find(table, key_to_insert);
    
    if (cursor->cell_num < num_cells) {
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert) {
            free(cursor);
            return EXECUTE_DUPLICATE_KEY;
        }
    }
    
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
    
    // Update secondary indexes  <-- ADD
    if (index_manager) {
        SecondaryIndex* username_idx = index_manager_get(index_manager, "users", "username");
        if (username_idx) {
            secondary_index_insert(username_idx, row_to_insert->username, row_to_insert->id);
        }
        
        SecondaryIndex* email_idx = index_manager_get(index_manager, "users", "email");
        if (email_idx) {
            secondary_index_insert(email_idx, row_to_insert->email, row_to_insert->id);
        }
    }
    
    // Log to WAL
    if (table->wal) {
        void* page = pager_get_page(table->pager, cursor->page_num);
        wal_write_frame(table->wal, cursor->page_num, page, table->pager->num_pages);
    }
    
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_create_table(ParsedStatement* stmt) {
    if (!global_schema) {
        global_schema = schema_create();
    }
    
    if (schema_add_table(global_schema, stmt->table_name, 
                         stmt->columns, stmt->num_columns)) {
        printf("Table '%s' created successfully.\n", stmt->table_name);
        return EXECUTE_SUCCESS;
    } else {
        return EXECUTE_TABLE_FULL;
    }
}

ExecuteResult execute_join(ParsedStatement* stmt, uint32_t* rows_returned_out) {
    if (!stmt->has_join) {
        return EXECUTE_SUCCESS;
    }
    
    // Open both tables
    Table* left_table = table_manager_get(table_manager, stmt->join_clause->left_table);
    Table* right_table = table_manager_get(table_manager, stmt->join_clause->right_table);
    
    if (!left_table) {
        left_table = table_manager_open(table_manager, stmt->join_clause->left_table);
    }
    if (!right_table) {
        right_table = table_manager_open(table_manager, stmt->join_clause->right_table);
    }
    
    if (!left_table || !right_table) {
        printf("Error: Could not open tables for JOIN\n");
        return EXECUTE_NOT_FOUND;
    }
    
    printf("Performing INNER JOIN on %s.%s = %s.%s\n", 
           stmt->join_clause->left_table, stmt->join_clause->left_column,
           stmt->join_clause->right_table, stmt->join_clause->right_column);
    
    uint32_t matches = 0;
    
    // Nested loop join (simple implementation)
    Cursor* left_cursor = table_start(left_table);
    while (!left_cursor->end_of_table) {
        Row left_row;
        deserialize_row(cursor_value(left_cursor), &left_row);
        
        Cursor* right_cursor = table_start(right_table);
        while (!right_cursor->end_of_table) {
            Row right_row;
            deserialize_row(cursor_value(right_cursor), &right_row);
            
            // Compare join columns (assuming id for now)
            bool match = false;
            if (strcmp(stmt->join_clause->left_column, "id") == 0 &&
                strcmp(stmt->join_clause->right_column, "user_id") == 0) {
                match = (left_row.id == right_row.id);
            } else if (strcmp(stmt->join_clause->left_column, "id") == 0 &&
                       strcmp(stmt->join_clause->right_column, "id") == 0) {
                match = (left_row.id == right_row.id);
            }
            
            if (match) {
                printf("%s: (%d, %s, %s) | %s: (%d, %s, %s)\n",
                       stmt->join_clause->left_table,
                       left_row.id, left_row.username, left_row.email,
                       stmt->join_clause->right_table,
                       right_row.id, right_row.username, right_row.email);
                matches++;
            }
            
            cursor_advance(right_cursor);
        }
        free(right_cursor);
        
        cursor_advance(left_cursor);
    }
    free(left_cursor);
    
    if (rows_returned_out) {
        *rows_returned_out = matches;
    }
    
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(ParsedStatement* stmt, Table* table, uint32_t* rows_returned_out) {
    if (stmt->has_join) {
        return execute_join(stmt, rows_returned_out);
    }

    Cursor* cursor = NULL;
    Row row;
    uint32_t rows_returned = 0;

    // Handle aggregations
    if (stmt->has_aggregation) {
        uint32_t count = 0;
        uint32_t sum = 0;
        uint32_t max_val = 0;
        uint32_t min_val = UINT32_MAX;
        
        cursor = table_start(table);
        while (!cursor->end_of_table) {
            deserialize_row(cursor_value(cursor), &row);
            
            bool include = true;
            if (stmt->has_where) {
                if (strcmp(stmt->where_clause->column, "id") == 0) {
                    uint32_t target_id = atoi(stmt->where_clause->value);
                    include = (row.id == target_id);
                } else if (strcmp(stmt->where_clause->column, "username") == 0) {
                    include = (strcmp(row.username, stmt->where_clause->value) == 0);
                } else if (strcmp(stmt->where_clause->column, "email") == 0) {
                    include = (strcmp(row.email, stmt->where_clause->value) == 0);
                }
            }
            
            if (include) {
                count++;
                if (strcmp(stmt->agg_column, "id") == 0 || strcmp(stmt->agg_column, "*") == 0) {
                    sum += row.id;
                    if (row.id > max_val) max_val = row.id;
                    if (row.id < min_val) min_val = row.id;
                }
            }
            
            cursor_advance(cursor);
        }
        free(cursor);
        
        // Print result based on aggregation type
        switch (stmt->agg_type) {
            case AGG_COUNT:
                printf("COUNT: %u\n", count);
                break;
            case AGG_SUM:
                printf("SUM: %u\n", sum);
                break;
            case AGG_AVG:
                if (count > 0) {
                    printf("AVG: %.2f\n", (float)sum / count);
                } else {
                    printf("AVG: 0\n");
                }
                break;
            case AGG_MAX:
                if (count > 0) {
                    printf("MAX: %u\n", max_val);
                } else {
                    printf("MAX: NULL\n");
                }
                break;
            case AGG_MIN:
                if (count > 0) {
                    printf("MIN: %u\n", min_val);
                } else {
                    printf("MIN: NULL\n");
                }
                break;
            default:
                break;
        }
        
        if (rows_returned_out) {
            *rows_returned_out = 1;
        }
        return EXECUTE_SUCCESS;
    }

    if (stmt->has_where && index_manager) {
        SecondaryIndex* index = index_manager_get(index_manager, "users", stmt->where_clause->column);
        
        if (index) {
            printf("Using secondary index on %s\n", stmt->where_clause->column);
            
            uint32_t count = 0;
            uint32_t* primary_keys = secondary_index_lookup(index, stmt->where_clause->value, &count);
            
            if (primary_keys) {
                for (uint32_t i = 0; i < count; i++) {
                    cursor = table_find(table, primary_keys[i]);
                    if (!cursor->end_of_table) {
                        deserialize_row(cursor_value(cursor), &row);
                        printf("(%d, %s, %s)\n", row.id, row.username, row.email);
                        rows_returned++;
                    }
                    free(cursor);
                }
                free(primary_keys);
                
                if (rows_returned_out) {
                    *rows_returned_out = rows_returned;
                }
                return EXECUTE_SUCCESS;
            }
        }
    }
    
    // Regular SELECT (non-aggregation)
    // Collect all rows first if ORDER BY is needed
    Row* rows_buffer = NULL;
    uint32_t buffer_size = 0;
    
    if (stmt->has_order_by) {
        rows_buffer = malloc(sizeof(Row) * 1000); // Max 1000 rows for sorting
    }
    
    if (stmt->has_where && strcmp(stmt->where_clause->column, "id") == 0) {
        uint32_t key = atoi(stmt->where_clause->value);
        cursor = table_find(table, key);
        
        if (!cursor->end_of_table) {
            void* node = pager_get_page(table->pager, cursor->page_num);
            if (cursor->cell_num < *leaf_node_num_cells(node)) {
                uint32_t key_at_cursor = *leaf_node_key(node, cursor->cell_num);
                if (key_at_cursor == key) {
                    deserialize_row(cursor_value(cursor), &row);
                    
                    if (stmt->has_order_by) {
                        rows_buffer[buffer_size++] = row;
                    } else {
                        printf("(%d, %s, %s)\n", row.id, row.username, row.email);
                        rows_returned++;
                    }
                }
            }
        }
        free(cursor);
    } else {
        cursor = table_start(table);
        while (!cursor->end_of_table) {
            deserialize_row(cursor_value(cursor), &row);
            
            bool should_include = true;
            if (stmt->has_where) {
                if (strcmp(stmt->where_clause->column, "username") == 0) {
                    should_include = (strcmp(row.username, stmt->where_clause->value) == 0);
                } else if (strcmp(stmt->where_clause->column, "email") == 0) {
                    should_include = (strcmp(row.email, stmt->where_clause->value) == 0);
                }
            }
            
            if (should_include) {
                if (stmt->has_order_by) {
                    rows_buffer[buffer_size++] = row;
                } else {
                    printf("(%d, %s, %s)\n", row.id, row.username, row.email);
                    rows_returned++;
                    
                    if (stmt->has_limit && rows_returned >= stmt->limit) {
                        break;
                    }
                }
            }
            
            cursor_advance(cursor);
        }
        free(cursor);
    }
    
    // Sort if ORDER BY is specified
    if (stmt->has_order_by && buffer_size > 0) {
        // Simple bubble sort (for demonstration)
        for (uint32_t i = 0; i < buffer_size - 1; i++) {
            for (uint32_t j = 0; j < buffer_size - i - 1; j++) {
                bool should_swap = false;
                
                if (strcmp(stmt->order_by_column, "id") == 0) {
                    if (stmt->order_ascending) {
                        should_swap = (rows_buffer[j].id > rows_buffer[j + 1].id);
                    } else {
                        should_swap = (rows_buffer[j].id < rows_buffer[j + 1].id);
                    }
                } else if (strcmp(stmt->order_by_column, "username") == 0) {
                    int cmp = strcmp(rows_buffer[j].username, rows_buffer[j + 1].username);
                    if (stmt->order_ascending) {
                        should_swap = (cmp > 0);
                    } else {
                        should_swap = (cmp < 0);
                    }
                }
                
                if (should_swap) {
                    Row temp = rows_buffer[j];
                    rows_buffer[j] = rows_buffer[j + 1];
                    rows_buffer[j + 1] = temp;
                }
            }
        }
        
        // Print sorted results
        uint32_t limit = stmt->has_limit ? stmt->limit : buffer_size;
        for (uint32_t i = 0; i < buffer_size && i < limit; i++) {
            printf("(%d, %s, %s)\n", rows_buffer[i].id, rows_buffer[i].username, rows_buffer[i].email);
            rows_returned++;
        }
        
        free(rows_buffer);
    }
    
    if (rows_returned_out) {
        *rows_returned_out = rows_returned;
    }
    
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(ParsedStatement* stmt, Table* table) {
    if (!stmt->has_where) {
        printf("UPDATE requires WHERE clause\n");
        return EXECUTE_SUCCESS;
    }
    
    Cursor* cursor = NULL;
    Row row;
    bool found = false;
    
    if (strcmp(stmt->where_clause->column, "id") == 0) {
        // Update by ID
        uint32_t key = atoi(stmt->where_clause->value);
        cursor = table_find(table, key);
        
        if (!cursor->end_of_table) {
            void* node = pager_get_page(table->pager, cursor->page_num);
            if (cursor->cell_num < *leaf_node_num_cells(node)) {
                uint32_t key_at_cursor = *leaf_node_key(node, cursor->cell_num);
                if (key_at_cursor == key) {
                    deserialize_row(cursor_value(cursor), &row);
                    
                    // Apply update
                    if (strcmp(stmt->assignments[0].column, "username") == 0) {
                        strncpy(row.username, stmt->assignments[0].value, COLUMN_USERNAME_SIZE);
                    } else if (strcmp(stmt->assignments[0].column, "email") == 0) {
                        strncpy(row.email, stmt->assignments[0].value, COLUMN_EMAIL_SIZE);
                    }
                    
                    serialize_row(&row, cursor_value(cursor));
                    found = true;
                }
            }
        }
        free(cursor);
    }
    
    return found ? EXECUTE_SUCCESS : EXECUTE_NOT_FOUND;
}

ExecuteResult execute_delete(ParsedStatement* stmt, Table* table) {
    if (!stmt->has_where) {
        printf("DELETE requires WHERE clause (DELETE ALL not supported)\n");
        return EXECUTE_SUCCESS;
    }
    
    Cursor* cursor = NULL;
    bool found = false;
    
    if (strcmp(stmt->where_clause->column, "id") == 0) {
        // Delete by ID
        uint32_t key = atoi(stmt->where_clause->value);
        cursor = table_find(table, key);
        
        if (!cursor->end_of_table) {
            void* node = pager_get_page(table->pager, cursor->page_num);
            if (cursor->cell_num < *leaf_node_num_cells(node)) {
                uint32_t key_at_cursor = *leaf_node_key(node, cursor->cell_num);
                if (key_at_cursor == key) {
                    leaf_node_delete(cursor);
                    
                    // Log to WAL
                    if (table->wal) {
                        void* page = pager_get_page(table->pager, cursor->page_num);
                        wal_write_frame(table->wal, cursor->page_num, page, table->pager->num_pages);
                    }
                    
                    found = true;
                }
            }
        }
    }
    
    free(cursor);
    return found ? EXECUTE_SUCCESS : EXECUTE_NOT_FOUND;
}

ExecuteResult execute_statement(ParsedStatement* stmt, Table* table) {
    if (stmt->type == STMT_CREATE_TABLE) {
        return execute_create_table(stmt);
    }

    if (stmt->type == STMT_CREATE_INDEX) {
        return execute_create_index(stmt, table);
    }

    QueryPlan* plan = optimize_query(stmt, table);

    if (stmt->is_explain) {
        print_query_plan(plan);
        free_query_plan(plan);
        return EXECUTE_SUCCESS;
    }
    
    ExecuteResult result;
    uint32_t actual_rows = 0;
    
    switch (stmt->type) {
        case STMT_INSERT:
            result = execute_insert(stmt, table);
            actual_rows = (result == EXECUTE_SUCCESS) ? 1 : 0;
            break;
        case STMT_SELECT:
            result = execute_select(stmt, table, &actual_rows);
            break;
        case STMT_UPDATE:
            result = execute_update(stmt, table);
            actual_rows = (result == EXECUTE_SUCCESS) ? 1 : 0;
            break;
        case STMT_DELETE:
            result = execute_delete(stmt, table);
            actual_rows = 0;
            break;
        case STMT_CREATE_TABLE:  
        case STMT_CREATE_INDEX:
            result = EXECUTE_SUCCESS;
            break;
    }
    
    if (global_stats && result == EXECUTE_SUCCESS) {
        stats_update(global_stats, plan, actual_rows);
    }
    
    free_query_plan(plan);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Must supply a database filename.\n");
        exit(EXIT_FAILURE);
    }
    
    char* filename = argv[1];
    Table* table = table_open(filename);
    
    global_stats = stats_create();
    global_schema = schema_load(filename);
    table_manager = table_manager_create(filename);  // <-- ADD
    index_manager = index_manager_create();
    
    InputBuffer* input_buffer = new_input_buffer();
    
    while (true) {
        print_prompt();
        read_input(input_buffer);
        
        if (input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer, table)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_UNRECOGNIZED:
                    printf("Unrecognized command '%s'\n", input_buffer->buffer);
                    continue;
            }
        }
        
        ParsedStatement* stmt = parse_statement(input_buffer->buffer);
        
        if (stmt == NULL) {
            printf("Syntax error. Could not parse statement.\n");
            continue;
        }
        
        ExecuteResult result = execute_statement(stmt, table);
        free_parsed_statement(stmt);
        
        switch (result) {
            case EXECUTE_SUCCESS:
                printf("Executed.\n");
                break;
            case EXECUTE_DUPLICATE_KEY:
                printf("Error: Duplicate key.\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error: Table full.\n");
                break;
            case EXECUTE_NOT_FOUND:
                printf("Error: Row not found.\n");
                break;
        }
    }
}
