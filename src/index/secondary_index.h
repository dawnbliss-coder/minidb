#ifndef SECONDARY_INDEX_H
#define SECONDARY_INDEX_H

#include <stdint.h>
#include <stdbool.h>
#include "../storage/table.h"

#define MAX_INDEXES 4
#define INDEX_KEY_SIZE 64

typedef struct {
    char key[INDEX_KEY_SIZE];  // The indexed value (e.g., username)
    uint32_t primary_key;       // Points to the primary key (id)
} IndexEntry;

typedef struct {
    char column_name[32];
    char table_name[64];
    IndexEntry* entries;
    uint32_t num_entries;
    uint32_t capacity;
} SecondaryIndex;

typedef struct {
    SecondaryIndex indexes[MAX_INDEXES];
    uint32_t num_indexes;
} IndexManager;

// Function declarations
IndexManager* index_manager_create();
void index_manager_free(IndexManager* manager);
bool index_manager_create_index(IndexManager* manager, const char* table_name, const char* column_name);
SecondaryIndex* index_manager_get(IndexManager* manager, const char* table_name, const char* column_name);
bool secondary_index_insert(SecondaryIndex* index, const char* key, uint32_t primary_key);
uint32_t* secondary_index_lookup(SecondaryIndex* index, const char* key, uint32_t* count);
void secondary_index_delete(SecondaryIndex* index, const char* key, uint32_t primary_key);
void secondary_index_print(SecondaryIndex* index);
bool index_manager_build_from_table(IndexManager* manager, const char* table_name, 
                                     const char* column_name, Table* table);

#endif // SECONDARY_INDEX_H
