#include "wal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h> 

#define WAL_MAGIC 0x377F0682
#define WAL_VERSION 1

static uint32_t wal_checksum(uint32_t* data, int count, uint32_t s1, uint32_t s2) {
    uint32_t sum1 = s1;
    uint32_t sum2 = s2;
    
    for (int i = 0; i < count; i++) {
        sum1 += data[i] + sum2;
        sum2 += data[i] + sum1;
    }
    
    return sum1 ^ sum2;
}

WAL* wal_open(const char* filename) {
    WAL* wal = malloc(sizeof(WAL));
    memset(wal, 0, sizeof(WAL));
    
    char wal_filename[256];
    snprintf(wal_filename, sizeof(wal_filename), "%s-wal", filename);
    
    wal->fd = open(wal_filename, O_RDWR | O_CREAT, 0644);
    if (wal->fd == -1) {
        free(wal);
        return NULL;
    }
    
    // Try to read existing header
    ssize_t bytes_read = read(wal->fd, &wal->header, sizeof(WALHeader));
    
    if (bytes_read < (ssize_t)sizeof(WALHeader) || wal->header.magic != WAL_MAGIC) {
        // Initialize new WAL file
        wal->header.magic = WAL_MAGIC;
        wal->header.version = WAL_VERSION;
        wal->header.page_size = PAGE_SIZE;
        wal->header.checkpoint_seq = 0;
        wal->header.salt1 = (uint32_t)time(NULL);
        wal->header.salt2 = (uint32_t)getpid();
        wal->header.checksum1 = 0;
        wal->header.checksum2 = 0;
        
        lseek(wal->fd, 0, SEEK_SET);
        write(wal->fd, &wal->header, sizeof(WALHeader));
    }
    
    wal->is_open = true;
    wal->frame_count = 0;
    
    return wal;
}

void wal_close(WAL* wal) {
    if (!wal) return;
    
    if (wal->is_open) {
        close(wal->fd);
        wal->is_open = false;
    }
    
    free(wal);
}

bool wal_write_frame(WAL* wal, uint32_t page_num, void* page_data, uint32_t db_size) {
    if (!wal || !wal->is_open) {
        return false;
    }
    
    WALFrame frame;
    frame.header.page_number = page_num;
    frame.header.db_size = db_size;
    frame.header.salt1 = wal->header.salt1;
    frame.header.salt2 = wal->header.salt2;
    
    // Calculate checksum
    frame.header.checksum1 = wal_checksum((uint32_t*)page_data, 
                                          PAGE_SIZE / sizeof(uint32_t),
                                          frame.header.salt1, 
                                          frame.header.salt2);
    frame.header.checksum2 = wal_checksum((uint32_t*)&frame.header, 
                                          sizeof(WALFrameHeader) / sizeof(uint32_t) - 2,
                                          frame.header.checksum1, 
                                          0);
    
    // Write frame header
    lseek(wal->fd, 0, SEEK_END);
    if (write(wal->fd, &frame.header, sizeof(WALFrameHeader)) != sizeof(WALFrameHeader)) {
        return false;
    }
    
    // Write page data
    if (write(wal->fd, page_data, PAGE_SIZE) != PAGE_SIZE) {
        return false;
    }
    
    wal->frame_count++;
    fsync(wal->fd);  // Force write to disk
    
    return true;
}

bool wal_checkpoint(WAL* wal, Pager* pager) {
    if (!wal || !wal->is_open || !pager) {
        return false;
    }
    
    printf("Checkpointing WAL (%u frames)...\n", wal->frame_count);
    
    // Flush all pages from pager to database file
    for (uint32_t i = 0; i < pager->num_pages; i++) {
        if (pager->pages[i] != NULL) {
            pager_flush(pager, i);
        }
    }
    
    // Truncate WAL file
    ftruncate(wal->fd, sizeof(WALHeader));
    wal->frame_count = 0;
    wal->header.checkpoint_seq++;
    
    lseek(wal->fd, 0, SEEK_SET);
    write(wal->fd, &wal->header, sizeof(WALHeader));
    fsync(wal->fd);
    
    printf("Checkpoint complete.\n");
    return true;
}

bool wal_recover(WAL* wal, Pager* pager) {
    if (!wal || !wal->is_open || !pager) {
        return false;
    }
    
    printf("Recovering from WAL...\n");
    
    lseek(wal->fd, sizeof(WALHeader), SEEK_SET);
    
    uint32_t frames_recovered = 0;
    WALFrameHeader frame_header;
    void* page_data = malloc(PAGE_SIZE);
    
    while (true) {
        ssize_t header_read = read(wal->fd, &frame_header, sizeof(WALFrameHeader));
        if (header_read < (ssize_t)sizeof(WALFrameHeader)) {
            break;
        }
        
        ssize_t data_read = read(wal->fd, page_data, PAGE_SIZE);
        if (data_read < PAGE_SIZE) {
            break;
        }
        
        // Verify checksum
        uint32_t checksum = wal_checksum((uint32_t*)page_data, 
                                         PAGE_SIZE / sizeof(uint32_t),
                                         frame_header.salt1, 
                                         frame_header.salt2);
        
        if (checksum == frame_header.checksum1) {
            // Apply frame to pager
            void* page = pager_get_page(pager, frame_header.page_number);
            memcpy(page, page_data, PAGE_SIZE);
            frames_recovered++;
        } else {
            printf("WAL frame %u checksum mismatch, stopping recovery.\n", frames_recovered);
            break;
        }
    }
    
    free(page_data);
    
    printf("Recovered %u frames from WAL.\n", frames_recovered);
    
    // Checkpoint after recovery
    wal_checkpoint(wal, pager);
    
    return true;
}

void wal_begin_transaction(WAL* wal) {
    if (!wal) return;
    printf("BEGIN TRANSACTION\n");
}

void wal_commit_transaction(WAL* wal) {
    if (!wal) return;
    printf("COMMIT TRANSACTION\n");
    fsync(wal->fd);
}

void wal_rollback_transaction(WAL* wal) {
    if (!wal) return;
    printf("ROLLBACK TRANSACTION\n");
    // In a full implementation, we'd undo changes here
}
