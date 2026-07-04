#include "secondary_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

IndexManager* index_manager_create() {
    IndexManager* manager = malloc(sizeof(IndexManager));
    memset(manager, 0, sizeof(IndexManager));
    return manager;
}

void index_manager_free(IndexManager* manager) {
    for (uint32_t i = 0; i < manager->num_indexes; i++) {
        if (manager->indexes[i].entries) {
            free(manager->indexes[i].entries);
        }
    }
    free(manager);
}

bool index_manager_create_index(IndexManager* manager, const char* table_name, const char* column_name) {
    if (manager->num_indexes >= MAX_INDEXES) {
        printf("Error: Maximum number of indexes reached\n");
        return false;
    }
    
    // Check if index already exists
    for (uint32_t i = 0; i < manager->num_indexes; i++) {
        if (strcmp(manager->indexes[i].table_name, table_name) == 0 &&
            strcmp(manager->indexes[i].column_name, column_name) == 0) {
            printf("Error: Index already exists on %s.%s\n", table_name, column_name);
            return false;
        }
    }
    
    SecondaryIndex* index = &manager->indexes[manager->num_indexes];
    strncpy(index->table_name, table_name, 63);
    strncpy(index->column_name, column_name, 31);
    index->capacity = 100;
    index->entries = malloc(sizeof(IndexEntry) * index->capacity);
    index->num_entries = 0;
    
    manager->num_indexes++;
    
    printf("Created index on %s.%s\n", table_name, column_name);
    return true;
}

SecondaryIndex* index_manager_get(IndexManager* manager, const char* table_name, const char* column_name) {
    for (uint32_t i = 0; i < manager->num_indexes; i++) {
        if (strcmp(manager->indexes[i].table_name, table_name) == 0 &&
            strcmp(manager->indexes[i].column_name, column_name) == 0) {
            return &manager->indexes[i];
        }
    }
    return NULL;
}

bool secondary_index_insert(SecondaryIndex* index, const char* key, uint32_t primary_key) {
    // Check if we need to resize
    if (index->num_entries >= index->capacity) {
        index->capacity *= 2;
        index->entries = realloc(index->entries, sizeof(IndexEntry) * index->capacity);
    }
    
    // Insert new entry (keeping it sorted for faster lookup)
    uint32_t insert_pos = index->num_entries;
    for (uint32_t i = 0; i < index->num_entries; i++) {
        if (strcmp(key, index->entries[i].key) < 0) {
            insert_pos = i;
            break;
        }
    }
    
    // Shift entries to make room
    for (uint32_t i = index->num_entries; i > insert_pos; i--) {
        index->entries[i] = index->entries[i - 1];
    }
    
    // Insert the new entry
    strncpy(index->entries[insert_pos].key, key, INDEX_KEY_SIZE - 1);
    index->entries[insert_pos].key[INDEX_KEY_SIZE - 1] = '\0';
    index->entries[insert_pos].primary_key = primary_key;
    index->num_entries++;
    
    return true;
}

uint32_t* secondary_index_lookup(SecondaryIndex* index, const char* key, uint32_t* count) {
    *count = 0;
    
    // Binary search for the key
    int left = 0;
    int right = index->num_entries - 1;
    int found_pos = -1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = strcmp(key, index->entries[mid].key);
        
        if (cmp == 0) {
            found_pos = mid;
            break;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    if (found_pos == -1) {
        return NULL;
    }
    
    // Count how many entries match (in case of duplicates)
    uint32_t start = found_pos;
    uint32_t end = found_pos;
    
    while (start > 0 && strcmp(index->entries[start - 1].key, key) == 0) {
        start--;
    }
    
    while (end < index->num_entries - 1 && strcmp(index->entries[end + 1].key, key) == 0) {
        end++;
    }
    
    *count = end - start + 1;
    uint32_t* results = malloc(sizeof(uint32_t) * (*count));
    
    for (uint32_t i = 0; i < *count; i++) {
        results[i] = index->entries[start + i].primary_key;
    }
    
    return results;
}

void secondary_index_delete(SecondaryIndex* index, const char* key, uint32_t primary_key) {
    for (uint32_t i = 0; i < index->num_entries; i++) {
        if (strcmp(index->entries[i].key, key) == 0 && 
            index->entries[i].primary_key == primary_key) {
            // Shift entries left
            for (uint32_t j = i; j < index->num_entries - 1; j++) {
                index->entries[j] = index->entries[j + 1];
            }
            index->num_entries--;
            return;
        }
    }
}

void secondary_index_print(SecondaryIndex* index) {
    printf("\nIndex on %s.%s (%u entries):\n", 
           index->table_name, index->column_name, index->num_entries);
    
    for (uint32_t i = 0; i < index->num_entries; i++) {
        printf("  '%s' -> id=%u\n", 
               index->entries[i].key, 
               index->entries[i].primary_key);
    }
    printf("\n");
}

bool index_manager_build_from_table(IndexManager* manager, const char* table_name, 
                                     const char* column_name, Table* table) {
    SecondaryIndex* index = index_manager_get(manager, table_name, column_name);
    if (!index) {
        return false;
    }
    
    printf("Building index on %s.%s...\n", table_name, column_name);
    
    Cursor* cursor = table_start(table);
    uint32_t count = 0;
    
    while (!cursor->end_of_table) {
        Row row;
        deserialize_row(cursor_value(cursor), &row);
        
        // Index the appropriate column
        if (strcmp(column_name, "username") == 0) {
            secondary_index_insert(index, row.username, row.id);
        } else if (strcmp(column_name, "email") == 0) {
            secondary_index_insert(index, row.email, row.id);
        }
        
        count++;
        cursor_advance(cursor);
    }
    
    free(cursor);
    
    printf("Index built: %u entries indexed\n", count);
    return true;
}
