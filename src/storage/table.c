#include "table.h"
#include "../index/btree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>    
#include <unistd.h>   

// Convert row to bytes for storage
void serialize_row(Row* source, void* destination) {
    memcpy(destination, &(source->id), ID_SIZE);
    memcpy(destination + ID_SIZE, &(source->username), USERNAME_SIZE);
    memcpy(destination + ID_SIZE + USERNAME_SIZE, &(source->email), EMAIL_SIZE);
}

// Convert bytes back to row
void deserialize_row(void* source, Row* destination) {
    memcpy(&(destination->id), source, ID_SIZE);
    memcpy(&(destination->username), source + ID_SIZE, USERNAME_SIZE);
    memcpy(&(destination->email), source + ID_SIZE + USERNAME_SIZE, EMAIL_SIZE);
}

Table* table_open(const char* filename) {
    Pager* pager = pager_open(filename);
    Table* table = malloc(sizeof(Table));
    table->pager = pager;
    
    // Open WAL
    table->wal = wal_open(filename);
    if (!table->wal) {
        printf("Warning: Could not open WAL file.\n");
    } else {
        // Recover from WAL if needed
        if (table->wal->frame_count > 0) {
            wal_recover(table->wal, pager);
        }
    }
    
    if (pager->num_pages == 0) {
        // New database file. Initialize page 0 as leaf node
        void* root_node = pager_get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_node_root(root_node, true);
        pager->num_pages = 1;
    }
    
    table->root_page_num = 0;
    return table;
}

void table_close(Table* table) {
    Pager* pager = table->pager;
    
    // Checkpoint WAL before closing
    if (table->wal) {
        wal_checkpoint(table->wal, pager);
        wal_close(table->wal);
    }
    
    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] == NULL) {
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }
    
    int result = close(pager->file_descriptor);
    if (result == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }
    
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        void* page = pager->pages[i];
        if (page) {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    
    free(pager);
    free(table);
}

Cursor* table_start(Table* table) {
    Cursor* cursor = table_find(table, 0);
    
    void* node = pager_get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    cursor->end_of_table = (num_cells == 0);
    
    return cursor;
}

Cursor* table_end(Table* table) {
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    
    void* root_node = pager_get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->cell_num = num_cells;
    cursor->end_of_table = true;
    
    return cursor;
}

void* cursor_value(Cursor* cursor) {
    uint32_t page_num = cursor->page_num;
    void* page = pager_get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor* cursor) {
    uint32_t page_num = cursor->page_num;
    void* node = pager_get_page(cursor->table->pager, page_num);
    
    cursor->cell_num += 1;
    if (cursor->cell_num >= (*leaf_node_num_cells(node))) {
        /* Advance to next leaf node */
        uint32_t next_page_num = *leaf_node_next_leaf(node);
        if (next_page_num == 0) {
            /* This was rightmost leaf */
            cursor->end_of_table = true;
        } else {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}
