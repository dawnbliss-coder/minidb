# MiniDB

MiniDB is a small SQL database implemented from scratch in C.  
The goal of this project is to learn how databases work internally: storage engine, B+Tree indexing, query optimization, transaction management, and crash recovery.

---

## Features

**SQL Operations**

- **DDL**: `CREATE TABLE`, `CREATE INDEX`
- **DML**: `SELECT`, `INSERT`, `UPDATE`, `DELETE`
- **Joins**: `INNER JOIN` with multi-table support
- **Query Modifiers**: `WHERE`, `ORDER BY`, `LIMIT`
- **Aggregations**: `COUNT()`, `SUM()`, `AVG()`, `MAX()`, `MIN()`
- **Query Analysis**: `EXPLAIN` for execution plans

**Storage Engine**

- B+Tree index on primary key with automatic node splitting
- Secondary indexes on any column for fast lookups
- Page-based storage (4 KB pages) with efficient caching
- Multi-table support with schema persistence
- Custom on-disk file format

**Transaction Management**

- Write-Ahead Log (WAL) for crash recovery
- Automatic checkpoint and log compaction
- Durability guarantees with fsync()
- Transaction statistics tracking

**Query Optimization**

- Cost-based optimizer (index search vs full scan)
- Secondary index utilization
- Query plan visualization with `EXPLAIN`
- Performance statistics collection

This is a single binary, file-backed database built for learning database internals.

---

## Build and Run

Requirements: `gcc` (C99), `make`, Linux/macOS/WSL.

```bash
git clone https://github.com/<your-username>/minidb.git
cd minidb
make

./minidb database.db
```

The database file will be created if it doesn't exist.

---

## Quick Demo

### Basic Operations

```sql
minidb> create table users (id int primary key, username varchar(32), email varchar(64))
Table 'users' created successfully.

minidb> insert 1 alice alice@example.com
Executed.

minidb> insert 2 bob bob@example.com
Executed.

minidb> insert 3 charlie charlie@example.com
Executed.

-- Point lookup using B+Tree
minidb> select * where id = 2
(2, bob, bob@example.com)
Executed.

-- Update records
minidb> update set email = bob_new@example.com where id = 2
Executed.

-- Delete records
minidb> delete where id = 3
Executed.

minidb> select count(*)
COUNT: 2
Executed.
```

### Secondary Indexes

```sql
-- Create index for fast username lookups
minidb> create index on users (username)
Created index on users.username
Building index on users.username...
Index built: 2 entries indexed
Executed.

-- Use secondary index (O(log n) instead of O(n))
minidb> select * where username = alice
Using secondary index on username
(1, alice, alice@example.com)
Executed.
```

### Aggregations

```sql
minidb> select count(*)
COUNT: 2

minidb> select sum(id)
SUM: 3

minidb> select avg(id)
AVG: 1.50

minidb> select max(id)
MAX: 2

minidb> select min(id)
MIN: 1
```

### Sorting and Limiting

```sql
-- Order by descending
minidb> select * order by id desc
(2, bob, bob_new@example.com)
(1, alice, alice@example.com)
Executed.

-- Limit results
minidb> select * order by id desc limit 1
(2, bob, bob_new@example.com)
Executed.
```

### Joins (Multi-Table)

```sql
minidb> create table orders (id int primary key, user_id int, product varchar(32))

minidb> insert 100 1 laptop
minidb> insert 101 2 mouse
minidb> insert 102 1 keyboard

-- Inner join between users and orders
minidb> select * from users inner join orders on users.id = orders.user_id
(1, alice, alice@example.com, 100, 1, laptop)
(2, bob, bob_new@example.com, 101, 2, mouse)
(1, alice, alice@example.com, 102, 1, keyboard)
Executed.
```

### Query Optimization

```sql
minidb> explain select * where id = 1

=== Query Plan ===
Scan Type: INDEX SEARCH (B+Tree)
Index Used: id (Primary Key)
Estimated Rows: 1
Estimated Cost: 5 (O(log n) - Binary Search)
==================

minidb> explain select * where email = 'test@example.com'

=== Query Plan ===
Scan Type: FULL TABLE SCAN
Estimated Rows: ALL
Estimated Cost: 250 (O(n) - Sequential Scan)
Recommendation: Consider creating an index on 'email'
==================
```

### Meta Commands

```sql
.schema       -- Show all table schemas
.btree        -- Display B+Tree structure
.stats        -- Show query execution statistics
.indexes      -- List all secondary indexes
.checkpoint   -- Force WAL checkpoint
.constants    -- Display internal constants
.exit         -- Exit database
```

---

## Architecture (High Level)

```text
SQL Query
    ↓
┌─────────────────┐
│  Lexer + Parser │  → Tokenize and parse SQL
└─────────────────┘
    ↓
┌─────────────────┐
│ Query Optimizer │  → Choose index vs full scan
└─────────────────┘
    ↓
┌─────────────────┐
│    Executor     │  → Execute SELECT/INSERT/UPDATE/DELETE/JOIN
└─────────────────┘
    ↓
┌─────────────────────────────────────┐
│  Storage Layer                      │
│  ┌────────┐ ┌────────┐ ┌────────┐ │
│  │ B+Tree │ │ Pager  │ │  WAL   │ │
│  └────────┘ └────────┘ └────────┘ │
└─────────────────────────────────────┘
    ↓
  Disk I/O
```

**Components:**

- `src/main.c` – REPL, meta commands, and CLI interface
- `src/parser/` – Lexer and recursive descent parser for SQL
- `src/storage/` – Table, pager, schema, multi-table manager
- `src/index/` – B+Tree (primary) and secondary index structures
- `src/transaction/` – Write-ahead logging and recovery
- `src/optimizer/` – Cost estimation and query plan generation

---

## Technical Details

### B+Tree Implementation

- **Structure**: Self-balancing tree with data in leaf nodes
- **Page Size**: 4 KB (4096 bytes)
- **Leaf Capacity**: 13 cells (key + serialized row)
- **Operations**: All O(log n) - insert, search, delete, update
- **Node Splitting**: Automatic when capacity exceeded

**Example Tree:**
```
        [Root: 10, 20]
       /       |          [1,5,8]  [10,15]  [20,25,30]  ← Leaf nodes with data
```

### Secondary Indexes

- Hash-based index mapping column value → primary key
- Binary search within index for O(log m) lookup
- Two-step process: index lookup → B+Tree primary key lookup
- Total cost: O(log m + log n)

### Write-Ahead Logging

**Frame Format:**
```
[Header: 24 bytes] [Page Data: 4096 bytes]
- page_number (4 bytes)
- db_size (4 bytes)
- salt1, salt2 (8 bytes)
- checksum1, checksum2 (8 bytes)
```

**Recovery Process:**
1. On startup, scan WAL file
2. Verify checksums for each frame
3. Replay valid frames to restore state
4. Checkpoint periodically to compact log

### Query Optimizer

**Statistics Tracked:**
- Full scans vs index searches
- Average rows scanned per query
- Query efficiency ratio

---


---

## Project Structure

```
minidb/
├── src/
│   ├── main.c                  # CLI and REPL
│   ├── parser/
│   │   ├── lexer.c            # SQL tokenization
│   │   └── parser.c           # AST generation
│   ├── storage/
│   │   ├── table.c            # Table operations
│   │   ├── pager.c            # Page cache management
│   │   ├── schema.c           # Schema persistence
│   │   └── table_manager.c    # Multi-table support
│   ├── index/
│   │   ├── btree.c            # B+Tree implementation
│   │   └── secondary_index.c  # Secondary indexes
│   ├── transaction/
│   │   └── wal.c              # Write-ahead logging
│   └── optimizer/
│       └── optimizer.c        # Query optimization
├── Makefile
└── README.md
```

---

## Testing

Create test files in `tests/` directory:

**tests/basic_ops.sql:**
```sql
create table test (id int primary key, name varchar(32), value varchar(64))
insert 1 test1 value1
insert 2 test2 value2
insert 3 test3 value3
select count(*)
select * order by id desc
update set value = updated where id = 2
delete where id = 3
select count(*)
.exit
```

**Run tests:**
```bash
./minidb test.db < tests/basic_ops.sql
```

---

## Why This Project

This project demonstrates understanding of:

- **Storage Systems**: Custom on-disk layouts, page management, caching
- **Data Structures**: B+Tree implementation with automatic balancing
- **Query Processing**: Parsing, optimization, execution
- **Transaction Management**: WAL, crash recovery, durability
- **Systems Programming**: Low-level C, file I/O, memory management

It showcases systems programming skills relevant to database, storage, and distributed systems roles.

---

## Future Enhancements

Possible extensions (not yet implemented):

- [ ] LEFT/RIGHT/OUTER JOIN support
- [ ] Subqueries and nested SELECT
- [ ] GROUP BY and HAVING clauses
- [ ] Transactions with BEGIN/COMMIT/ROLLBACK
- [ ] Multi-threading for concurrent queries
- [ ] Client-server architecture
- [ ] B-tree node compression
- [ ] Query result caching

---
