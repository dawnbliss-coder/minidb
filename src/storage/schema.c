#include "schema.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

Schema* schema_create() {
    Schema* schema = malloc(sizeof(Schema));
    memset(schema, 0, sizeof(Schema));
    return schema;
}

void schema_free(Schema* schema) {
    free(schema);
}

bool schema_add_table(Schema* schema, const char* table_name, 
                      ColumnDef* columns, uint32_t num_columns) {
    if (schema->num_tables >= MAX_TABLES) {
        printf("Error: Maximum number of tables reached\n");
        return false;
    }
    
    // Check if table already exists
    for (uint32_t i = 0; i < schema->num_tables; i++) {
        if (strcmp(schema->tables[i].name, table_name) == 0) {
            printf("Error: Table '%s' already exists\n", table_name);
            return false;
        }
    }
    
    TableSchema* table = &schema->tables[schema->num_tables];
    strncpy(table->name, table_name, MAX_TABLE_NAME);
    table->num_columns = num_columns;
    
    // Find primary key
    table->primary_key_index = 0;
    for (uint32_t i = 0; i < num_columns; i++) {
        table->columns[i] = columns[i];
        if (columns[i].is_primary_key) {
            table->primary_key_index = i;
        }
    }
    
    schema->num_tables++;
    return true;
}

TableSchema* schema_get_table(Schema* schema, const char* table_name) {
    for (uint32_t i = 0; i < schema->num_tables; i++) {
        if (strcmp(schema->tables[i].name, table_name) == 0) {
            return &schema->tables[i];
        }
    }
    return NULL;
}

void schema_print(Schema* schema) {
    printf("\n=== Database Schema ===\n");
    printf("Tables: %u\n\n", schema->num_tables);
    
    for (uint32_t i = 0; i < schema->num_tables; i++) {
        TableSchema* table = &schema->tables[i];
        printf("Table: %s\n", table->name);
        printf("Columns:\n");
        
        for (uint32_t j = 0; j < table->num_columns; j++) {
            ColumnDef* col = &table->columns[j];
            printf("  - %s ", col->name);
            
            switch (col->type) {
                case TYPE_INT:
                    printf("INT");
                    break;
                case TYPE_VARCHAR:
                    printf("VARCHAR(%u)", col->size);
                    break;
                case TYPE_TEXT:
                    printf("TEXT");
                    break;
            }
            
            if (col->is_primary_key) {
                printf(" PRIMARY KEY");
            }
            if (col->not_null) {
                printf(" NOT NULL");
            }
            printf("\n");
        }
        printf("\n");
    }
    printf("=====================\n\n");
}

bool schema_save(Schema* schema, const char* filename) {
    char schema_filename[256];
    snprintf(schema_filename, sizeof(schema_filename), "%s.schema", filename);
    
    int fd = open(schema_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        return false;
    }
    
    write(fd, schema, sizeof(Schema));
    close(fd);
    return true;
}

Schema* schema_load(const char* filename) {
    char schema_filename[256];
    snprintf(schema_filename, sizeof(schema_filename), "%s.schema", filename);
    
    int fd = open(schema_filename, O_RDONLY);
    if (fd == -1) {
        // No schema file, create new
        return schema_create();
    }
    
    Schema* schema = malloc(sizeof(Schema));
    ssize_t bytes_read = read(fd, schema, sizeof(Schema));
    close(fd);
    
    if (bytes_read < (ssize_t)sizeof(Schema)) {
        free(schema);
        return schema_create();
    }
    
    return schema;
}
