#ifndef TABLE_MANAGER_H
#define TABLE_MANAGER_H

#include "table.h"
#include "schema.h"

#define MAX_OPEN_TABLES 8

typedef struct {
    Table* tables[MAX_OPEN_TABLES];
    char table_names[MAX_OPEN_TABLES][64];
    uint32_t num_tables;
    char base_path[256];
} TableManager;

TableManager* table_manager_create(const char* base_path);
void table_manager_free(TableManager* manager);
Table* table_manager_open(TableManager* manager, const char* table_name);
Table* table_manager_get(TableManager* manager, const char* table_name);
void table_manager_close_all(TableManager* manager);

#endif // TABLE_MANAGER_H
