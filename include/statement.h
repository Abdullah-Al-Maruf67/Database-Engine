#ifndef STATEMENT_H
#define STATEMENT_H

#include "common.h"
#include "row.h"
#include "table.h"
#include "input_buffer.h"

typedef enum {
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL,
  EXECUTE_DUPLICATE_KEY
} ExecuteResult;

typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
  PREPARE_SUCCESS,
  PREPARE_SYNTAX_ERROR,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_STRING_TOO_LONG,
  PREPARE_NEGATIVE_ID
} PrepareResult;

typedef enum {
  STATEMENT_INSERT,
  STATEMENT_SELECT,
  STATEMENT_UPDATE,
  STATEMENT_DELETE,
  STATEMENT_CREATE_TABLE,
  STATEMENT_CREATE_INDEX
} StatementType;

#include "schema.h"

#define MAX_VALUES 32

typedef union {
  int32_t int_value;
  char* string_value;
} Value;

typedef struct {
  char table_name[MAX_TABLE_NAME];
  TableSchema schema;
} CreateTableStatement;

typedef struct {
  char table_name[MAX_TABLE_NAME];
  char index_name[MAX_INDEX_NAME];
  char column_name[MAX_COLUMN_NAME];
} CreateIndexStatement;


typedef struct {
  char table_name[MAX_TABLE_NAME];
  Value values[MAX_VALUES];
  uint32_t num_values;
} InsertStatement;

#include "expression.h"


typedef struct {
  char table_name[MAX_TABLE_NAME];
  char join_table_name[MAX_TABLE_NAME];
  char join_on_column[MAX_COLUMN_NAME];
  char join_with_column[MAX_COLUMN_NAME];
  Expression* where_clause;
} SelectStatement;

typedef struct {
  char table_name[MAX_TABLE_NAME];
  Value values[MAX_VALUES];
  uint32_t num_values;
  uint32_t set_mask; // Bitmask for which columns are set
  Expression* where_clause;
} UpdateStatement;

typedef struct {
  char table_name[MAX_TABLE_NAME];
  Expression* where_clause;
} DeleteStatement;

typedef struct {
  StatementType type;
  union {
    CreateTableStatement create_table;
    CreateIndexStatement create_index;
    InsertStatement insert;
    SelectStatement select;
    UpdateStatement update; 
    DeleteStatement delete;
  };
} Statement;



MetaCommandResult do_meta_command(InputBuffer* input_buffer, Catalog* catalog, FILE* out);
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement, Catalog* catalog);
ExecuteResult execute_statement(Statement* statement, Catalog* catalog, FILE* out);

ExecuteResult execute_delete(Statement* statement, Catalog* catalog, FILE* out);
ExecuteResult execute_update(Statement* statement, Catalog* catalog, FILE* out);




#endif
