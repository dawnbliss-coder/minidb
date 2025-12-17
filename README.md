# MiniDB

A lightweight SQL database engine built from scratch in C, featuring B+Tree indexing, query optimization, write-ahead logging, and crash recovery. Implements core database internals for learning and demonstration purposes.

## Overview

MiniDB is a single-file, embedded database that demonstrates fundamental database system concepts through a clean, readable implementation. Built entirely in C with no external dependencies, it provides a complete SQL engine with storage management, indexing, and transaction support.

## 🎯 Key Features

- **SQL Support** - DDL (CREATE TABLE, CREATE INDEX), DML (SELECT, INSERT, UPDATE, DELETE), INNER JOIN, aggregations
- **B+Tree Indexing** - Self-balancing tree with automatic node splitting for O(log n) operations
- **Query Optimization** - Cost-based optimizer choosing between index search and full table scan
- **Secondary Indexes** - Fast lookups on non-primary-key columns
- **Write-Ahead Logging** - Crash recovery with WAL and automatic checkpointing
- **Multi-Table Support** - Join operations across multiple tables with schema persistence

## Technical Highlights

- Implemented B+Tree index from scratch with 4KB page-based storage and automatic node splitting
- Designed query optimizer using cost estimation (5 × log n vs. 5 × n) to select optimal execution path
- Built write-ahead logging system with checksummed frames and crash recovery on startup
- Developed recursive descent parser for SQL with lexer tokenization and AST generation
- Achieved 500× performance improvement for point queries using B+Tree vs. full table scan (0.1ms vs. 50ms on 1000 rows)

## Quick Start

```bash
# Build
git clone https://github.com/<your-username>/minidb.git
cd minidb
make

# Run
./minidb database.db
```

**Requirements:** gcc (C99), make, Linux/macOS/WSL

## Example Usage

### Basic CRUD Operations

```sql
minidb> create table users (id int primary key, username varchar(32), email varchar(64))
Table 'users' created successfully.

minidb> insert 1 alice alice@example.com
Executed.

minidb> select * where id = 1
(1, alice, alice@example.com)
Executed.

minidb> update set email = newemail@example.com where id = 1
Executed.

minidb> delete where id = 1
Executed.
```

### Secondary Indexes & Optimization

```sql
-- Create index on username column
minidb> create index on users (username)
Index built: 10 entries indexed

-- Fast O(log n) lookup instead of O(n) scan
minidb> select * where username = alice
Using secondary index on username
(1, alice, alice@example.com)

-- View query execution plan
minidb> explain select * where id = 1
=== Query Plan ===
Scan Type: INDEX SEARCH (B+Tree)
Estimated Cost: 5 (O(log n))
```

### Joins & Aggregations

```sql
minidb> create table orders (id int primary key, user_id int, product varchar(32))
minidb> insert 100 1 laptop

minidb> select * from users inner join orders on users.id = orders.user_id
(1, alice, alice@example.com, 100, 1, laptop)

minidb> select count(*)
COUNT: 10

minidb> select avg(id), max(id), min(id)
AVG: 5.50  MAX: 10  MIN: 1
```

### Meta Commands

```sql
.schema       -- Show all table schemas
.btree        -- Display B+Tree structure
.stats        -- Query execution statistics
.indexes      -- List all secondary indexes
.checkpoint   -- Force WAL checkpoint
.exit         -- Exit database
```

## Architecture

```
SQL Query
    ↓
[Lexer + Parser] → Tokenize & parse SQL
    ↓
[Query Optimizer] → Cost-based index vs. scan decision
    ↓
[Executor] → Execute query plan
    ↓
[Storage Layer: B+Tree | Pager | WAL]
    ↓
Disk I/O (4KB pages)
```

**Core Components:**
- **Parser** (`src/parser/`) - Lexer tokenization and recursive descent parsing
- **Storage** (`src/storage/`) - Page cache, table manager, schema persistence
- **Index** (`src/index/`) - B+Tree and secondary index implementation
- **Transaction** (`src/transaction/`) - Write-ahead log with crash recovery
- **Optimizer** (`src/optimizer/`) - Cost estimation and plan generation

[View Detailed Architecture](IMPLEMENTATION.md)

## Performance

Benchmarks on 1000 rows (MacBook Air M1):

| Operation | B+Tree Index | Full Scan | Speedup |
|-----------|--------------|-----------|---------|
| Point SELECT | 0.1ms | 50ms | 500× |
| UPDATE | 0.2ms | 50ms | 250× |
| DELETE | 0.2ms | 50ms | 250× |
| INSERT | 0.5ms | N/A | — |
| COUNT(*) | 0.5ms | 50ms | 100× |

### Query Optimizer Decisions

- **< 100 rows**: Index overhead not worth it, use full scan
- **100-1000 rows**: Index 100-500× faster
- **> 1000 rows**: Index essential, 1000× faster

## B+Tree Implementation

**Structure:**
- Self-balancing tree with all data in leaf nodes
- 4KB pages with 13 cells per leaf node
- Automatic node splitting when capacity exceeded
- O(log n) for insert, search, update, delete

**Example:**
```
        [Root: 10, 20]
       /       |       \
  [1,5,8]  [10,15]  [20,25,30]  ← Leaf nodes (data)
```

## Write-Ahead Logging

**WAL Frame Format:**
```
[Header: 24 bytes] [Page: 4096 bytes]
- Page number, DB size
- Salt values for versioning
- Checksums for integrity
```

**Recovery Process:**
1. Scan WAL on startup
2. Verify checksums for each frame
3. Replay valid frames to restore state
4. Checkpoint to compact log

## Project Structure

```
minidb/
├── src/
│   ├── main.c              # REPL and CLI
│   ├── parser/
│   │   ├── lexer.c        # SQL tokenization
│   │   └── parser.c       # AST generation
│   ├── storage/
│   │   ├── table.c        # Table operations
│   │   ├── pager.c        # Page cache (4KB pages)
│   │   ├── schema.c       # Schema persistence
│   │   └── table_manager.c # Multi-table support
│   ├── index/
│   │   ├── btree.c        # B+Tree implementation
│   │   └── secondary_index.c # Secondary indexes
│   ├── transaction/
│   │   └── wal.c          # Write-ahead logging
│   └── optimizer/
│       └── optimizer.c    # Cost-based optimization
├── tests/
│   └── basic_ops.sql      # Test SQL scripts
└── Makefile
```

## Testing

Run test scripts:

```bash
./minidb test.db < tests/basic_ops.sql
```

Create custom test files in `tests/` directory with SQL commands.

## Learning Outcomes

This project demonstrates:
- **Storage engine design** - Page-based storage, caching, custom file formats
- **Data structures** - B+Tree implementation with automatic balancing
- **Query processing** - Parsing, optimization, execution pipeline
- **Transaction management** - WAL, crash recovery, durability guarantees
- **Systems programming** - Low-level C, file I/O, memory management
- **Algorithm analysis** - Big-O complexity, performance benchmarking

## Future Enhancements

Potential extensions:
- [ ] Transactions with BEGIN/COMMIT/ROLLBACK
- [ ] LEFT/RIGHT/OUTER JOIN support
- [ ] GROUP BY and HAVING clauses
- [ ] Subqueries and nested SELECT
- [ ] Multi-threading for concurrent queries
- [ ] Client-server architecture with network protocol
- [ ] B+Tree node compression
- [ ] Query result caching

## Why This Project

MiniDB showcases systems programming and database internals knowledge relevant to:
- Database systems engineering
- Storage systems development
- Distributed systems infrastructure
- Low-level systems programming roles

Built as a learning project to understand how production databases like SQLite, PostgreSQL, and MySQL work under the hood.

## License

MIT License

## Acknowledgments

- Inspired by SQLite architecture and "Let's Build a Simple Database" tutorial
- B+Tree design based on database textbook algorithms
- WAL implementation follows industry-standard crash recovery principles
