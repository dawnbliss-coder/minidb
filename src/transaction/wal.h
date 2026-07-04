#ifndef WAL_H
#define WAL_H

#include <stdint.h>
#include <stdbool.h>
#include "../storage/pager.h"

#define WAL_HEADER_SIZE 32
#define WAL_FRAME_HEADER_SIZE 24

typedef enum {
    WAL_OP_INSERT,
    WAL_OP_UPDATE,
    WAL_OP_DELETE,
    WAL_OP_CHECKPOINT
} WALOpType;

typedef struct {
    uint32_t magic;           // Magic number for validation
    uint32_t version;         // WAL format version
    uint32_t page_size;       // Database page size
    uint32_t checkpoint_seq;  // Last checkpoint sequence number
    uint32_t salt1;           // Random salt for checksums
    uint32_t salt2;           // Random salt for checksums
    uint32_t checksum1;       // Cumulative checksum
    uint32_t checksum2;       // Cumulative checksum
} WALHeader;

typedef struct {
    uint32_t page_number;     // Which page this frame modifies
    uint32_t db_size;         // Database size after this frame
    uint32_t salt1;           // Copy from header
    uint32_t salt2;           // Copy from header
    uint32_t checksum1;       // Frame checksum
    uint32_t checksum2;       // Frame checksum
} WALFrameHeader;

typedef struct {
    WALFrameHeader header;
    void* page_data;          // PAGE_SIZE bytes
} WALFrame;

typedef struct {
    int fd;                   // File descriptor for WAL file
    WALHeader header;
    uint32_t frame_count;     // Number of frames in WAL
    bool is_open;
} WAL;

// Function declarations
WAL* wal_open(const char* filename);
void wal_close(WAL* wal);
bool wal_write_frame(WAL* wal, uint32_t page_num, void* page_data, uint32_t db_size);
bool wal_checkpoint(WAL* wal, Pager* pager);
bool wal_recover(WAL* wal, Pager* pager);
void wal_begin_transaction(WAL* wal);
void wal_commit_transaction(WAL* wal);
void wal_rollback_transaction(WAL* wal);

#endif // WAL_H
