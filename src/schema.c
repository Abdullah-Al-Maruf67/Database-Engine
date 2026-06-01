#include "schema.h"

void schema_init(TableSchema* schema, const char* name) {
  memset(schema, 0, sizeof(TableSchema));
  strncpy(schema->table_name, name, MAX_TABLE_NAME);
  schema->num_columns = 0;
  schema->row_size = 0;
}

void schema_add_column(TableSchema* schema, const char* name, DataType type, uint32_t size) {
  if (schema->num_columns >= MAX_COLUMNS) return;
  
  Column* col = &schema->columns[schema->num_columns++];
  strncpy(col->name, name, MAX_COLUMN_NAME);
  col->type = type;
  col->size = size;
  
  // Fixed size calculation for non-slotted fields or for overall record estimation
  if (type == TYPE_INT) {
    schema->row_size += sizeof(int32_t);
  } else {
    schema->row_size += size;
  }
}

uint32_t schema_get_column_offset(TableSchema* schema, uint32_t column_idx) {
  uint32_t offset = 0;
  for (uint32_t i = 0; i < column_idx; i++) {
    if (schema->columns[i].type == TYPE_INT) {
      offset += sizeof(int32_t);
    } else {
      offset += schema->columns[i].size;
    }
  }
  return offset;
}
