#include "table_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TableManager* table_manager_create(const char* base_path) {
    TableManager* manager = malloc(sizeof(TableManager));
    memset(manager, 0, sizeof(TableManager));
    strncpy(manager->base_path, base_path, 255);
    return manager;
}

void table_manager_free(TableManager* manager) {
    table_manager_close_all(manager);
    free(manager);
}

Table* table_manager_open(TableManager* manager, const char* table_name) {
    // Check if already open
    Table* existing = table_manager_get(manager, table_name);
    if (existing) {
        return existing;
    }
    
    if (manager->num_tables >= MAX_OPEN_TABLES) {
        printf("Error: Maximum number of open tables reached\n");
        return NULL;
    }
    
    // Create filename: base_path.table_name
    char filename[512];
    snprintf(filename, sizeof(filename), "%s.%s", manager->base_path, table_name);
    
    Table* table = table_open(filename);
    if (!table) {
        return NULL;
    }
    
    strncpy(table->name, table_name, 63);
    manager->tables[manager->num_tables] = table;
    strncpy(manager->table_names[manager->num_tables], table_name, 63);
    manager->num_tables++;
    
    return table;
}

Table* table_manager_get(TableManager* manager, const char* table_name) {
    for (uint32_t i = 0; i < manager->num_tables; i++) {
        if (strcmp(manager->table_names[i], table_name) == 0) {
            return manager->tables[i];
        }
    }
    return NULL;
}

void table_manager_close_all(TableManager* manager) {
    for (uint32_t i = 0; i < manager->num_tables; i++) {
        if (manager->tables[i]) {
            table_close(manager->tables[i]);
            manager->tables[i] = NULL;
        }
    }
    manager->num_tables = 0;
}
