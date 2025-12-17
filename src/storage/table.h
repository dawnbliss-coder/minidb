#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include "pager.h"
#include "../transaction/wal.h"

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100

#define ID_SIZE sizeof(uint32_t)
#define USERNAME_SIZE COLUMN_USERNAME_SIZE
#define EMAIL_SIZE COLUMN_EMAIL_SIZE
#define ID_OFFSET 0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct {
    Pager* pager;
    uint32_t root_page_num;
    WAL* wal;
    char name[64];  
} Table;

typedef struct {
    Table* table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
} Cursor;

void serialize_row(Row* source, void* destination);
void deserialize_row(void* source, Row* destination);
void* cursor_value(Cursor* cursor);
Cursor* table_start(Table* table);
Cursor* table_find(Table* table, uint32_t key);
void cursor_advance(Cursor* cursor);
Table* table_open(const char* filename);
void table_close(Table* table);

#endif // TABLE_H
