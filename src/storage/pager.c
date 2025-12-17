#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

Pager* pager_open(const char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    
    if (fd == -1) {
        printf("Unable to open file: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    
    off_t file_length = lseek(fd, 0, SEEK_END);
    
    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = fd;
    pager->file_length = file_length;
    pager->num_pages = (file_length / PAGE_SIZE);
    
    if (file_length % PAGE_SIZE != 0) {
        printf("Database file is corrupted. Not a whole number of pages.\n");
        exit(EXIT_FAILURE);
    }
    
    // Initialize page pointers to NULL
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = NULL;
    }
    
    return pager;
}

Page* pager_get_page(Pager* pager, uint32_t page_num) {
    if (page_num > TABLE_MAX_PAGES) {
        printf("Tried to fetch page number out of bounds: %d > %d\n", 
               page_num, TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }
    
    // Check if page is already in cache
    if (pager->pages[page_num] == NULL) {
        // Cache miss - allocate memory and load from file
        Page* page = malloc(sizeof(Page));
        uint32_t num_pages = pager->file_length / PAGE_SIZE;
        
        // Save partial page at end of file
        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }
        
        if (page_num <= num_pages) {
            // Page exists in file - read it
            lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1) {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        
        pager->pages[page_num] = page;
        
        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
    }
    
    return pager->pages[page_num];
}

void pager_flush(Pager* pager, uint32_t page_num) {
    if (pager->pages[page_num] == NULL) {
        printf("Tried to flush null page\n");
        exit(EXIT_FAILURE);
    }
    
    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
    
    if (offset == -1) {
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    
    ssize_t bytes_written = write(pager->file_descriptor, 
                                   pager->pages[page_num], 
                                   PAGE_SIZE);
    
    if (bytes_written == -1) {
        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

void pager_close(Pager* pager) {
    // Flush all pages to disk
    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] != NULL) {
            pager_flush(pager, i);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }
    }
    
    close(pager->file_descriptor);
    free(pager);
}
