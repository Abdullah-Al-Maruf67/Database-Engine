#ifndef SCHEMA_H
#define SCHEMA_H

#include "common.h"

#define MAX_COLUMNS 32
#define MAX_TABLE_NAME 64
#define MAX_COLUMN_NAME 64
#define MAX_INDEX_NAME 64
#define MAX_INDEXES_PER_TABLE 10

typedef enum {
  TYPE_INT,
  TYPE_TEXT
} DataType;

typedef struct {
  char name[MAX_COLUMN_NAME];
  DataType type;
  uint32_t size;
} Column;

typedef struct {
  char name[MAX_INDEX_NAME];
  uint32_t column_idx;
  uint32_t root_page_num;
} IndexSchema;

typedef struct {
  char table_name[MAX_TABLE_NAME];
  Column columns[MAX_COLUMNS];
  uint32_t num_columns;
  IndexSchema indexes[MAX_INDEXES_PER_TABLE];
  uint32_t num_indexes;
  uint32_t row_size;
} TableSchema;


void schema_init(TableSchema* schema, const char* name);
void schema_add_column(TableSchema* schema, const char* name, DataType type, uint32_t size);
uint32_t schema_get_column_offset(TableSchema* schema, uint32_t column_idx);

#endif
