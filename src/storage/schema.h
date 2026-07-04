#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TABLE_NAME 32
#define MAX_COLUMN_NAME 32
#define MAX_COLUMNS 16
#define MAX_TABLES 8

typedef enum {
    TYPE_INT,
    TYPE_VARCHAR,
    TYPE_TEXT
} ColumnType;

typedef struct {
    char name[MAX_COLUMN_NAME];
    ColumnType type;
    uint32_t size;
    bool is_primary_key;
    bool not_null;
} ColumnDef;

typedef struct {
    char name[MAX_TABLE_NAME];
    ColumnDef columns[MAX_COLUMNS];
    uint32_t num_columns;
    uint32_t primary_key_index;
} TableSchema;

typedef struct {
    TableSchema tables[MAX_TABLES];
    uint32_t num_tables;
} Schema;

// Function declarations
Schema* schema_create();
void schema_free(Schema* schema);
bool schema_add_table(Schema* schema, const char* table_name, 
                      ColumnDef* columns, uint32_t num_columns);
TableSchema* schema_get_table(Schema* schema, const char* table_name);
void schema_print(Schema* schema);
bool schema_save(Schema* schema, const char* filename);
Schema* schema_load(const char* filename);

#endif // SCHEMA_H
