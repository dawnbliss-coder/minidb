#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100

// A page is the basic unit of storage
typedef struct {
    char data[PAGE_SIZE];
} Page;

// Pager manages pages and file I/O
typedef struct {
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    Page* pages[TABLE_MAX_PAGES];
} Pager;

// Function declarations
Pager* pager_open(const char* filename);
void pager_close(Pager* pager);
Page* pager_get_page(Pager* pager, uint32_t page_num);
void pager_flush(Pager* pager, uint32_t page_num);

#endif // PAGER_H
