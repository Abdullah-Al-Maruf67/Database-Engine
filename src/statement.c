#include "statement.h"
#include "evaluator.h"
#include "input_buffer.h"
#include "parser.h"


MetaCommandResult do_meta_command(InputBuffer* input_buffer, Catalog* catalog, FILE* out) {


  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    db_close(catalog);
    exit(EXIT_SUCCESS);
  } else if (strcmp(input_buffer->buffer, ".btree") == 0) {
    for (uint32_t i = 0; i < catalog->num_tables; i++) {
      Table* table = &catalog->tables[i];
      fprintf(out, "Table: %s (Root Page: %d)\n", table->schema.table_name, table->root_page_num);
      print_tree(catalog->pager, table->root_page_num, 0, out);
      
      for (uint32_t j = 0; j < table->schema.num_indexes; j++) {
        fprintf(out, "Index: %s (Root Page: %d)\n", table->schema.indexes[j].name, table->schema.indexes[j].root_page_num);
        print_tree(catalog->pager, table->schema.indexes[j].root_page_num, 0, out);
      }
    }
    return META_COMMAND_SUCCESS;
  } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
    fprintf(out, "Constants:\n");
    print_constants(out);
    return META_COMMAND_SUCCESS;
  } else if (strcmp(input_buffer->buffer, ".schema") == 0) {
    for (uint32_t i = 0; i < catalog->num_tables; i++) {
      TableSchema* schema = &catalog->tables[i].schema;
      fprintf(out, "Table: %s\n", schema->table_name);
      for (uint32_t j = 0; j < schema->num_columns; j++) {
        fprintf(out, "  %s: %s\n", schema->columns[j].name,
               schema->columns[j].type == TYPE_INT ? "INT" : "TEXT");
      }
    }
    return META_COMMAND_SUCCESS;
  } else if (strcmp(input_buffer->buffer, ".stats") == 0) {
    fprintf(out, "Database Statistics:\n");
    fprintf(out, "  Total Pages: %d\n", catalog->pager->num_pages);
    for (uint32_t i = 0; i < catalog->num_tables; i++) {
      fprintf(out, "  Table '%s': Depth %d, Rows %d\n", 
             catalog->tables[i].schema.table_name,
             get_tree_depth(&catalog->tables[i]),
             count_rows(&catalog->tables[i]));
    }
    return META_COMMAND_SUCCESS;
  } else if (strcmp(input_buffer->buffer, ".help") == 0) {
    fprintf(out, "Available commands:\n");
    fprintf(out, "  .exit - Exit the program\n");
    fprintf(out, "  .help - Print this help message\n");
    fprintf(out, "  .btree - Visualise the B-tree\n");
    fprintf(out, "  .schema - Show table schema\n");
    fprintf(out, "  .stats - Show database statistics\n");
    fprintf(out, "  CREATE TABLE <name> (<col> <type>, ...) - Create a table\n");
    fprintf(out, "  INSERT INTO <name> VALUES (<val>, ...) - Insert a row\n");
    fprintf(out, "  SELECT * FROM <name> - Print all rows\n");
    return META_COMMAND_SUCCESS;
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}


PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement,
                                Catalog* catalog) {
  Parser parser;
  parser_init(&parser, input_buffer->buffer, catalog);
  return parser_parse(&parser, statement);
}

static uint32_t dynamic_serialize(Statement* statement, TableSchema* schema, void* destination) {
  uint32_t offset = 0;
  for (uint32_t i = 0; i < schema->num_columns; i++) {
    if (schema->columns[i].type == TYPE_INT) {
      memcpy(destination + offset, &statement->insert.values[i].int_value, sizeof(int32_t));
      offset += sizeof(int32_t);
    } else {
      uint32_t len = strlen(statement->insert.values[i].string_value) + 1;
      memcpy(destination + offset, statement->insert.values[i].string_value, len);
      offset += len;
    }
  }
  return offset;
}

static void dynamic_print_row(void* source, TableSchema* schema, FILE* out) {
  uint32_t offset = 0;
  fprintf(out, "(");
  for (uint32_t i = 0; i < schema->num_columns; i++) {
    if (schema->columns[i].type == TYPE_INT) {
      int32_t val;
      memcpy(&val, source + offset, sizeof(int32_t));
      fprintf(out, "%d", val);
      offset += sizeof(int32_t);
    } else {
      char* val = (char*)(source + offset);
      fprintf(out, "%s", val);
      offset += strlen(val) + 1;
    }
    if (i < schema->num_columns - 1) fprintf(out, ", ");
  }
  fprintf(out, ")\n");
}


static void maintain_indexes_insert(Table* table, uint32_t primary_key, Statement* statement) {
  for (uint32_t i = 0; i < table->schema.num_indexes; i++) {
    IndexSchema* idx = &table->schema.indexes[i];
    Value* val = &statement->insert.values[idx->column_idx];
    
    // Create index entry: [IndexedValue][PrimaryKey]
    char entry_buffer[4096];
    uint32_t key_size;
    uint32_t btree_key;
    
    if (table->schema.columns[idx->column_idx].type == TYPE_INT) {
      key_size = sizeof(int32_t);
      memcpy(entry_buffer, &val->int_value, key_size);
      btree_key = (uint32_t)val->int_value;
    } else {
      key_size = strlen(val->string_value) + 1;
      memcpy(entry_buffer, val->string_value, key_size);
      // For TEXT, the B-tree key is just the first 4 bytes of the string for now.
      // This is not perfect for range scans but works for equality if we check later.
      memcpy(&btree_key, entry_buffer, sizeof(uint32_t));
    }
    
    memcpy(entry_buffer + key_size, &primary_key, sizeof(uint32_t));
    uint32_t entry_size = key_size + sizeof(uint32_t);
    
    // Create a temporary "table" structure to represent the index B-tree
    Table index_table;
    index_table.pager = table->pager;
    index_table.root_page_num = idx->root_page_num;
    // We don't really use the schema of the index table here yet
    
    Cursor* cursor = table_find(&index_table, btree_key);
    leaf_node_insert(cursor, btree_key, entry_buffer, entry_size);
    free(cursor);
  }
}

ExecuteResult execute_insert(Statement* statement, Catalog* catalog) {
  Table* table = catalog_get_table(catalog, statement->insert.table_name);
  if (table == NULL) {
    return EXECUTE_SUCCESS; // Or error
  }
  uint32_t key_to_insert = statement->insert.values[0].int_value; // Assume first col is primary key
  
  Cursor* cursor = table_find(table, key_to_insert);
  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  if (cursor->cell_num < num_cells) {
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_insert) {
      free(cursor);
      return EXECUTE_DUPLICATE_KEY;
    }
  }

  char temp_buffer[4096];
  uint32_t row_size = dynamic_serialize(statement, &table->schema, temp_buffer);
  leaf_node_insert(cursor, key_to_insert, temp_buffer, row_size);
  
  maintain_indexes_insert(table, key_to_insert, statement);
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Catalog* catalog, FILE* out) {
  Table* table = catalog_get_table(catalog, statement->select.table_name);
  if (table == NULL) return EXECUTE_SUCCESS;

  if (statement->select.join_table_name[0] != '\0') {
    // Nested Loop Join
    Table* join_table = catalog_get_table(catalog, statement->select.join_table_name);
    if (join_table == NULL) {
      fprintf(out, "Error: Join table '%s' not found\n", statement->select.join_table_name);
      return EXECUTE_SUCCESS;
    }

    // Find column indices for ON clause
    int left_col_idx = -1;
    for (uint32_t i = 0; i < table->schema.num_columns; i++) {
      if (strcasecmp(table->schema.columns[i].name, statement->select.join_on_column) == 0) {
        left_col_idx = i;
        break;
      }
    }
    int right_col_idx = -1;
    for (uint32_t i = 0; i < join_table->schema.num_columns; i++) {
      if (strcasecmp(join_table->schema.columns[i].name, statement->select.join_with_column) == 0) {
        right_col_idx = i;
        break;
      }
    }

    if (left_col_idx == -1 || right_col_idx == -1) {
      fprintf(out, "Error: JOIN column not found\n");
      return EXECUTE_SUCCESS;
    }

    Cursor* left_cursor = table_start(table);
    while (!(left_cursor->end_of_table)) {
      void* left_row = cursor_value(left_cursor);

      Cursor* right_cursor = table_start(join_table);
      while (!(right_cursor->end_of_table)) {
        void* right_row = cursor_value(right_cursor);

        // Extract join values
        // This is a bit complex due to variable length records.
        // For simplicity, let's just use get_column_value helper if it existed,
        // or re-implement it here.

        void* left_val_ptr = left_row;
        for (int i = 0; i < left_col_idx; i++) {
          if (table->schema.columns[i].type == TYPE_INT) left_val_ptr += sizeof(int32_t);
          else left_val_ptr += strlen((char*)left_val_ptr) + 1;
        }

        void* right_val_ptr = right_row;
        for (int i = 0; i < right_col_idx; i++) {
          if (join_table->schema.columns[i].type == TYPE_INT) right_val_ptr += sizeof(int32_t);
          else right_val_ptr += strlen((char*)right_val_ptr) + 1;
        }

        bool match = false;
        if (table->schema.columns[left_col_idx].type == TYPE_INT) {
          match = (*(int32_t*)left_val_ptr == *(int32_t*)right_val_ptr);
        } else {
          match = (strcmp((char*)left_val_ptr, (char*)right_val_ptr) == 0);
        }

        if (match) {
           // For now, evaluate where clause only on the first table
           // In a real DB, it would be on the joined result
           if (evaluate(statement->select.where_clause, left_row, &table->schema)) {
             fprintf(out, "[");
             dynamic_print_row(left_row, &table->schema, out);
             fprintf(out, " | ");
             dynamic_print_row(right_row, &join_table->schema, out);
             fprintf(out, "]\n");
           }
        }
        cursor_advance(right_cursor);
      }
      free(right_cursor);
      cursor_advance(left_cursor);
    }
    free(left_cursor);
    return EXECUTE_SUCCESS;
  }

  Expression* where = statement->select.where_clause;
  IndexSchema* best_idx = NULL;
  uint32_t search_key = 0;
  bool use_index = false;

  // Simple optimizer: look for equality on indexed column
  if (where && where->type == EXPR_BINARY_OP && where->binary.op == OP_EQ) {
    Expression* left = where->binary.left;
    Expression* right = where->binary.right;
    
    Expression* col_expr = NULL;
    Expression* lit_expr = NULL;
    
    if (left->type == EXPR_COLUMN && right->type == EXPR_LITERAL) {
      col_expr = left;
      lit_expr = right;
    } else if (right->type == EXPR_COLUMN && left->type == EXPR_LITERAL) {
      col_expr = right;
      lit_expr = left;
    }
    
    if (col_expr && lit_expr && lit_expr->literal.type == TYPE_INT) {
      for (uint32_t i = 0; i < table->schema.num_indexes; i++) {
        if (table->schema.indexes[i].column_idx == col_expr->column.column_idx) {
          best_idx = &table->schema.indexes[i];
          search_key = lit_expr->literal.int_value;
          use_index = true;
          break;
        }
      }
    }
  }

  if (use_index) {
    Table index_table;
    index_table.pager = table->pager;
    index_table.root_page_num = best_idx->root_page_num;
    
    Cursor* idx_cursor = table_find(&index_table, search_key);
    while (!idx_cursor->end_of_table) {
      void* entry = cursor_value(idx_cursor);
      uint32_t entry_key = *(uint32_t*)entry;
      if (entry_key != search_key) break;
      
      uint32_t primary_key = *(uint32_t*)(entry + sizeof(int32_t));
      
      // Now find the row in the main table
      Cursor* table_cursor = table_find(table, primary_key);
      void* node = get_page(table->pager, table_cursor->page_num);
      if (table_cursor->cell_num < *leaf_node_num_cells(node)) {
        if (*leaf_node_key(node, table_cursor->cell_num) == primary_key) {
           void* row = cursor_value(table_cursor);
           if (evaluate(where, row, &table->schema)) {
             dynamic_print_row(row, &table->schema, out);
           }
        }
      }
      free(table_cursor);
      
      cursor_advance(idx_cursor);
    }
    free(idx_cursor);
  } else {
    // Full table scan
    Cursor* cursor = table_start(table);
    while (!(cursor->end_of_table)) {
      void* row = cursor_value(cursor);
      if (evaluate(where, row, &table->schema)) {
        dynamic_print_row(row, &table->schema, out);
      }
      cursor_advance(cursor);
    }
    free(cursor);
  }
  return EXECUTE_SUCCESS;
}



ExecuteResult execute_create_table(Statement* statement, Catalog* catalog) {
  uint32_t root_page_num = get_unused_page_num(catalog->pager);
  void* root_node = get_page(catalog->pager, root_page_num);
  initialize_leaf_node(root_node);
  set_node_root(root_node, true);
  pager_flush(catalog->pager, root_page_num);
  
  uint32_t schema_page_num = get_unused_page_num(catalog->pager);
  void* schema_page = get_page(catalog->pager, schema_page_num);
  memcpy(schema_page, &statement->create_table.schema, sizeof(TableSchema));
  pager_flush(catalog->pager, schema_page_num);

  catalog_add_table(catalog, &statement->create_table.schema, root_page_num, schema_page_num);
  catalog_save(catalog);
  return EXECUTE_SUCCESS;
}
ExecuteResult execute_delete(Statement* statement, Catalog* catalog, FILE* out) {
  Table* table = catalog_get_table(catalog, statement->delete.table_name);
  Cursor* cursor = table_start(table);
  uint32_t deleted_count = 0;
  
  while (!(cursor->end_of_table)) {
    void* row = cursor_value(cursor);
    if (evaluate(statement->delete.where_clause, row, &table->schema)) {
      void* node = get_page(table->pager, cursor->page_num);
      uint32_t num_cells = *leaf_node_num_cells(node);
      
      /* Shift subsequent slots left */
      for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++) {
        memcpy(leaf_node_slot(node, i), leaf_node_slot(node, i + 1),
               LEAF_NODE_SLOT_SIZE);
      }
      *leaf_node_num_cells(node) -= 1;
      deleted_count++;
      
      /* After deletion, don't advance cursor as next element shifted into current slot */
      if (*leaf_node_num_cells(node) == 0) {
        cursor_advance(cursor);
      }
    } else {
      cursor_advance(cursor);
    }
  }
  free(cursor);
  fprintf(out, "Deleted %d rows\n", deleted_count);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_create_index(Statement* statement, Catalog* catalog) {
  Table* table = catalog_get_table(catalog, statement->create_index.table_name);
  if (table == NULL) {
    return EXECUTE_SUCCESS;
  }
  if (table->schema.num_indexes >= MAX_INDEXES_PER_TABLE) {
    printf("Error: Too many indexes\n");
    return EXECUTE_SUCCESS;
  }
  
  uint32_t root_page_num = get_unused_page_num(catalog->pager);
  void* root_node = get_page(catalog->pager, root_page_num);
  initialize_leaf_node(root_node);
  set_node_root(root_node, true);
  pager_flush(catalog->pager, root_page_num);
  
  IndexSchema* idx = &table->schema.indexes[table->schema.num_indexes++];
  strncpy(idx->name, statement->create_index.index_name, MAX_INDEX_NAME);
  idx->root_page_num = root_page_num;
  
  // Find column index
  for (uint32_t i = 0; i < table->schema.num_columns; i++) {
    if (strcasecmp(table->schema.columns[i].name, statement->create_index.column_name) == 0) {
      idx->column_idx = i;
      break;
    }
  }
  
  /* Backfill existing data into index */
  Cursor* cursor = table_start(table);
  while (!cursor->end_of_table) {
    void* row = cursor_value(cursor);
    uint32_t primary_key = *(uint32_t*)row; // Assume first col is primary key
    
    // Extract column value
    void* val_ptr = row;
    for (uint32_t i = 0; i < idx->column_idx; i++) {
      if (table->schema.columns[i].type == TYPE_INT) val_ptr += sizeof(int32_t);
      else val_ptr += strlen((char*)val_ptr) + 1;
    }
    
    char entry_buffer[4096];
    uint32_t key_size;
    uint32_t btree_key;
    
    if (table->schema.columns[idx->column_idx].type == TYPE_INT) {
      key_size = sizeof(int32_t);
      memcpy(entry_buffer, val_ptr, key_size);
      memcpy(&btree_key, val_ptr, sizeof(uint32_t));
    } else {
      key_size = strlen((char*)val_ptr) + 1;
      memcpy(entry_buffer, val_ptr, key_size);
      memcpy(&btree_key, entry_buffer, sizeof(uint32_t));
    }
    
    memcpy(entry_buffer + key_size, &primary_key, sizeof(uint32_t));
    uint32_t entry_size = key_size + sizeof(uint32_t);
    
    Table index_table;
    index_table.pager = table->pager;
    index_table.root_page_num = root_page_num;
    
    Cursor* idx_cursor = table_find(&index_table, btree_key);
    leaf_node_insert(idx_cursor, btree_key, entry_buffer, entry_size);
    free(idx_cursor);
    
    cursor_advance(cursor);
  }
  free(cursor);

  // Update schema on disk
  void* schema_page = get_page(catalog->pager, table->schema_page_num);
  memcpy(schema_page, &table->schema, sizeof(TableSchema));
  pager_flush(catalog->pager, table->schema_page_num);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Statement* statement, Catalog* catalog, FILE* out) {
  Table* table = catalog_get_table(catalog, statement->update.table_name);
  if (table == NULL) return EXECUTE_SUCCESS;
  
  Cursor* cursor = table_start(table);
  uint32_t updated_count = 0;
  
  while (!(cursor->end_of_table)) {
    void* row = cursor_value(cursor);
    if (evaluate(statement->update.where_clause, row, &table->schema)) {
      // Create new row data
      char new_row_data[4096];
      uint32_t offset = 0;
      uint32_t old_row_offset = 0;
      
      void* old_row_ptr = row;

      for (uint32_t i = 0; i < table->schema.num_columns; i++) {
        uint32_t current_old_col_size;
        if (table->schema.columns[i].type == TYPE_INT) {
           current_old_col_size = sizeof(int32_t);
        } else {
           current_old_col_size = strlen((char*)old_row_ptr) + 1;
        }

        if (statement->update.set_mask & (1 << i)) {
          // Use new value
          if (table->schema.columns[i].type == TYPE_INT) {
            memcpy(new_row_data + offset, &statement->update.values[i].int_value, sizeof(int32_t));
            offset += sizeof(int32_t);
          } else {
            uint32_t len = strlen(statement->update.values[i].string_value) + 1;
            memcpy(new_row_data + offset, statement->update.values[i].string_value, len);
            offset += len;
          }
        } else {
          // Copy from old row
          memcpy(new_row_data + offset, old_row_ptr, current_old_col_size);
          offset += current_old_col_size;
        }
        old_row_ptr += current_old_col_size;
        old_row_offset += current_old_col_size;
      }
      
      uint32_t new_row_size = offset;
      uint32_t old_row_size = old_row_offset;

      if (new_row_size == old_row_size) {
        memcpy(row, new_row_data, new_row_size);
        updated_count++;
      } else {
        fprintf(out, "Warning: UPDATE failed due to record size change (old: %d, new: %d). Only same-length updates supported for now.\n", old_row_size, new_row_size);
      }
    }
    cursor_advance(cursor);
  }
  free(cursor);
  fprintf(out, "Updated %d rows\n", updated_count);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Catalog* catalog, FILE* out) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      return execute_insert(statement, catalog);
    case (STATEMENT_SELECT):
      return execute_select(statement, catalog, out);
    case (STATEMENT_UPDATE):
      return execute_update(statement, catalog, out);
    case (STATEMENT_DELETE):
      return execute_delete(statement, catalog, out);
    case (STATEMENT_CREATE_TABLE):
      return execute_create_table(statement, catalog);
    case (STATEMENT_CREATE_INDEX):
      return execute_create_index(statement, catalog);
    default:
      fprintf(out, "Error: Statement type not supported yet\n");
      return EXECUTE_SUCCESS;
  }
}




