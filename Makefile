CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I./src
TARGET = minidb

OBJS = build/main.o \
       build/storage/pager.o \
       build/storage/table.o \
       build/storage/schema.o \
       build/storage/table_manager.o \
       build/index/btree.o \
       build/index/secondary_index.o \
       build/transaction/wal.o \
       build/optimizer/optimizer.o \
       build/parser/lexer.o \
       build/parser/parser.o

DIRS = build build/storage build/index build/transaction build/optimizer build/parser

all: $(DIRS) $(TARGET)

$(DIRS):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/storage/pager.o: src/storage/pager.c src/storage/pager.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/storage/table.o: src/storage/table.c src/storage/table.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/storage/schema.o: src/storage/schema.c src/storage/schema.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/storage/table_manager.o: src/storage/table_manager.c src/storage/table_manager.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/index/btree.o: src/index/btree.c src/index/btree.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/transaction/wal.o: src/transaction/wal.c src/transaction/wal.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/optimizer/optimizer.o: src/optimizer/optimizer.c src/optimizer/optimizer.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/parser/lexer.o: src/parser/lexer.c src/parser/lexer.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/parser/parser.o: src/parser/parser.c src/parser/parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/index/secondary_index.o: src/index/secondary_index.c src/index/secondary_index.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build $(TARGET)

.PHONY: all clean $(DIRS)
