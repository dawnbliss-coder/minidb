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
Table 'orders' created successfully.

minidb> insert 100 1 laptop
minidb> insert 101 2 mouse
minidb> insert 102 1 keyboard

-- Inner join between users and orders
minidb> select * from users inner join orders on users.id = orders.user_id
Performing INNER JOIN on users.id = orders.user_id
users: (1, alice, alice@example.com) | orders: (100, 1, laptop)
users: (1, alice, alice@example.com) | orders: (102, 1, keyboard)
users: (2, bob, bob_new@example.com) | orders: (101, 2, mouse)
Executed.
```

> Note: `CREATE TABLE` makes the new table "active", so statements right
> after `create table orders (...)` (like the inserts above) apply to
> `orders`, not `users`. Switch back with `.use users`, or just always
> use `FROM`/`JOIN` clauses to be explicit about which table you mean.

### Query Optimization

```sql
minidb> explain select * where id = 1

=== Query Plan ===
Scan Type: INDEX SEARCH (B+Tree)
Index Used: id (Primary Key)
Estimated Rows: 1
Estimated Cost: 5 (O(log n) - Binary Search)
==================

minidb> explain select * where email = test@example.com

=== Query Plan ===
Scan Type: FULL TABLE SCAN
Index Used: NONE (Sequential Scan)
Estimated Rows: 3
Estimated Cost: 15 (O(n) - Linear Scan)
==================
```

### Meta Commands

```sql
.schema       -- Show all table schemas
.btree        -- Display B+Tree structure of the active table
.stats        -- Show query execution statistics
.indexes      -- List all secondary indexes
.checkpoint   -- Force WAL checkpoint
.begin        -- Begin a WAL transaction
.commit       -- Commit a WAL transaction
.rollback     -- Roll back a WAL transaction (logs intent; see note below)
.use <table>  -- Switch the active table
.constants    -- Display internal constants
.exit         -- Exit database
```

> **Active table:** Statements like `insert`, `select`, `update`, and
> `delete` that don't include an explicit `FROM`/table target operate on
> the *active table* — whichever table was most recently created, or
> whichever you last switched to with `.use <table>`. This state also
> persists across restarts (MiniDB remembers the last active table).

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

---

## Technical Details

### B+Tree Implementation

- **Structure**: Self-balancing tree with data in leaf nodes
- **Page Size**: 4 KB (4096 bytes)
- **Leaf Capacity**: 13 cells (key + serialized row)
- **Internal Node Capacity**: 3 keys / 4 children per node before splitting
- **Operations**: All O(log n) - insert, search, delete, update
- **Node Splitting**: Automatic for both leaf and internal nodes, including
  recursive splits that propagate all the way up to the root. Verified
  correct (via Valgrind, zero errors/leaks) up to 2,000+ rows spanning
  multiple internal-node levels.
- **Max table size**: up to 100,000 pages (~400 MB per table at 4 KB/page)
  before hitting the configured `TABLE_MAX_PAGES` ceiling in
  `src/storage/pager.h`.

**Example Tree:**
```
        [Root: 10, 20]
       /       |
 [1,5,8]  [10,15]  [20,25,30]  ← Leaf nodes with data
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

## Known Limitations

MiniDB is a learning project, not a production database. A few things
worth knowing before you rely on it for anything real:

- **Fixed row shape.** Every table is physically stored as one integer
  primary key plus two string columns, regardless of the column types
  you declare in `CREATE TABLE`. Extra/differently-typed columns beyond
  that (e.g. a 4th column, or a `FLOAT`) aren't supported by the storage
  layer yet — `CREATE TABLE` schemas are used for display and column-name
  resolution (e.g. in `JOIN`/`WHERE`), but the on-disk layout is always
  `(id, col2, col3)`.
- **No `INSERT INTO <table> VALUES (...)` syntax.** Inserts are
  positional (`insert <id> <col2> <col3>`) and always target the
  *active* table (see the Meta Commands section above).
- **Single-condition `WHERE`.** No `AND`/`OR`/compound conditions.
- **`ORDER BY` sort buffer caps out at 1000 rows** (`rows_buffer` in
  `execute_select`).
---
