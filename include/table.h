#ifndef TABLE_H
#define TABLE_H

#include "common.h"
#include "pager.h"
#include "node.h"

#include "schema.h"

#define MAX_TABLES 10

typedef struct Table_t {
  Pager* pager;
  uint32_t root_page_num;
  uint32_t schema_page_num;
  TableSchema schema;
} Table;

typedef struct Cursor_t {
  Table* table;
  uint32_t page_num;
  uint32_t cell_num;
  bool end_of_table;
} Cursor;

typedef struct {
  Pager* pager;
  Table tables[MAX_TABLES];
  uint32_t num_tables;
  pthread_mutex_t db_mutex;
} Catalog;


Catalog* db_open(const char* filename);
void db_close(Catalog* catalog);

Table* catalog_get_table(Catalog* catalog, const char* table_name);
void catalog_add_table(Catalog* catalog, TableSchema* schema, uint32_t root_page_num, uint32_t schema_page_num);
void catalog_save(Catalog* catalog);

Cursor* table_start(Table* table);

Cursor* table_end(Table* table);
Cursor* table_find(Table* table, uint32_t key);
void* cursor_value(Cursor* cursor);

void cursor_advance(Cursor* cursor);

uint32_t get_tree_depth(Table* table);
uint32_t count_rows(Table* table);

#endif

