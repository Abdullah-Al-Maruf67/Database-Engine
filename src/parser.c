#include "parser.h"

void parser_init(Parser* parser, const char* source, Catalog* catalog) {
  memset(parser, 0, sizeof(Parser));
  tokenizer_init(&parser->tokenizer, source);
  parser->catalog = catalog;
  parser->had_error = false;
  parser->current_token = tokenizer_next_token(&parser->tokenizer);
  parser->previous_token.text = NULL;
}


static void advance(Parser* parser) {
  token_free(&parser->previous_token);
  parser->previous_token = parser->current_token;
  parser->current_token = tokenizer_next_token(&parser->tokenizer);
}

static bool check(Parser* parser, TokenType type) {
  return parser->current_token.type == type;
}

static bool match(Parser* parser, TokenType type) {
  if (!check(parser, type)) return false;
  advance(parser);
  return true;
}

static PrepareResult consume(Parser* parser, TokenType type, const char* message) {
  if (check(parser, type)) {
    advance(parser);
    return PREPARE_SUCCESS;
  }
  printf("Syntax Error: %s at '%s'\n", message, parser->current_token.text);
  parser->had_error = true;
  return PREPARE_SYNTAX_ERROR;
}

static PrepareResult parse_create_table(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_CREATE_TABLE;
  CreateTableStatement* stmt = &statement->create_table;
  
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  schema_init(&stmt->schema, stmt->table_name);
  
  if (consume(parser, TK_LPAREN, "Expect '(' before column definitions") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  do {
    if (consume(parser, TK_IDENTIFIER, "Expect column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    char col_name[MAX_COLUMN_NAME];
    strncpy(col_name, parser->previous_token.text, MAX_COLUMN_NAME);
    
    DataType type;
    uint32_t size = 0;
    if (match(parser, TK_INT)) {
      type = TYPE_INT;
    } else if (match(parser, TK_TEXT)) {
      type = TYPE_TEXT;
      if (match(parser, TK_LPAREN)) {
        if (consume(parser, TK_INTEGER_LITERAL, "Expect integer for TEXT size") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
        size = atoi(parser->previous_token.text);
        if (consume(parser, TK_RPAREN, "Expect ')' after size") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
      } else {
        size = 255; // Default size
      }
    } else {
      printf("Error: Expect column type (INT or TEXT)\n");
      return PREPARE_SYNTAX_ERROR;
    }
    
    schema_add_column(&stmt->schema, col_name, type, size);
  } while (match(parser, TK_COMMA));
  
  if (consume(parser, TK_RPAREN, "Expect ')' after column definitions") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  return PREPARE_SUCCESS;
}


static Expression* parse_expression(Parser* parser, TableSchema* schema);

static Expression* parse_primary(Parser* parser, TableSchema* schema) {
  if (match(parser, TK_INTEGER_LITERAL)) {
    return expr_new_int_literal(atoi(parser->previous_token.text));
  }
  if (match(parser, TK_STRING_LITERAL)) {
    return expr_new_string_literal(parser->previous_token.text);
  }
  if (match(parser, TK_IDENTIFIER)) {
    char* name = parser->previous_token.text;
    for (uint32_t i = 0; i < schema->num_columns; i++) {
      if (strcasecmp(schema->columns[i].name, name) == 0) {
        return expr_new_column(schema->columns[i].name, i);
      }
    }
    printf("Error: Column '%s' not found in table '%s'\n", name, schema->table_name);
    parser->had_error = true;
    return NULL;
  }
  if (match(parser, TK_LPAREN)) {
    Expression* expr = parse_expression(parser, schema);
    consume(parser, TK_RPAREN, "Expect ')' after expression");
    return expr;
  }
  
  printf("Error: Expect expression\n");
  parser->had_error = true;
  return NULL;
}

static Expression* parse_comparison(Parser* parser, TableSchema* schema) {
  Expression* expr = parse_primary(parser, schema);
  
  while (match(parser, TK_EQUALS) || match(parser, TK_GT) || match(parser, TK_LT) ||
         match(parser, TK_GE) || match(parser, TK_LE) || match(parser, TK_BANG_EQUALS)) {
    TokenType op_type = parser->previous_token.type;
    Operator op;
    switch (op_type) {
      case TK_EQUALS: op = OP_EQ; break;
      case TK_GT: op = OP_GT; break;
      case TK_LT: op = OP_LT; break;
      case TK_GE: op = OP_GE; break;
      case TK_LE: op = OP_LE; break;
      case TK_BANG_EQUALS: op = OP_NE; break;
      default: break;
    }
    Expression* right = parse_primary(parser, schema);
    expr = expr_new_binary(expr, op, right);
  }
  return expr;
}

static Expression* parse_and(Parser* parser, TableSchema* schema) {
  Expression* expr = parse_comparison(parser, schema);
  while (match(parser, TK_AND)) {
    Expression* right = parse_comparison(parser, schema);
    expr = expr_new_binary(expr, OP_AND, right);
  }
  return expr;
}

static Expression* parse_or(Parser* parser, TableSchema* schema) {
  Expression* expr = parse_and(parser, schema);
  while (match(parser, TK_OR)) {
    Expression* right = parse_and(parser, schema);
    expr = expr_new_binary(expr, OP_OR, right);
  }
  return expr;
}

static Expression* parse_expression(Parser* parser, TableSchema* schema) {
  return parse_or(parser, schema);
}

static PrepareResult parse_where(Parser* parser, TableSchema* schema, Expression** where_clause) {
  if (match(parser, TK_WHERE)) {
    *where_clause = parse_expression(parser, schema);
    if (parser->had_error) return PREPARE_SYNTAX_ERROR;
  } else {
    *where_clause = NULL;
  }
  return PREPARE_SUCCESS;
}

static PrepareResult parse_insert(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_INSERT;
  InsertStatement* stmt = &statement->insert;
  
  if (consume(parser, TK_INTO, "Expect 'INTO' after 'INSERT'") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  
  Table* table = catalog_get_table(parser->catalog, stmt->table_name);
  if (table == NULL) {
    printf("Error: Table '%s' does not exist\n", stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  if (consume(parser, TK_VALUES, "Expect 'VALUES'") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_LPAREN, "Expect '('") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  stmt->num_values = 0;
  do {
    if (stmt->num_values >= MAX_VALUES) {
      printf("Error: Too many values\n");
      return PREPARE_SYNTAX_ERROR;
    }
    
    if (match(parser, TK_INTEGER_LITERAL)) {
      stmt->values[stmt->num_values].int_value = atoi(parser->previous_token.text);
    } else if (match(parser, TK_STRING_LITERAL)) {
      stmt->values[stmt->num_values].string_value = strdup(parser->previous_token.text);
    } else {
      printf("Error: Expect literal value\n");
      return PREPARE_SYNTAX_ERROR;
    }
    stmt->num_values++;
  } while (match(parser, TK_COMMA));
  
  if (consume(parser, TK_RPAREN, "Expect ')'") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  if (stmt->num_values != table->schema.num_columns) {
    printf("Error: Column count mismatch (expected %d, got %d)\n", table->schema.num_columns, stmt->num_values);
    return PREPARE_SYNTAX_ERROR;
  }
  
  return PREPARE_SUCCESS;
}

static PrepareResult parse_select(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_SELECT;
  SelectStatement* stmt = &statement->select;
  stmt->join_table_name[0] = '\0';
  
  if (consume(parser, TK_STAR, "Expect '*' (only SELECT * supported for now)") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_FROM, "Expect 'FROM'") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  
  Table* table = catalog_get_table(parser->catalog, stmt->table_name);
  if (table == NULL) {
    printf("Error: Table '%s' does not exist\n", stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  if (match(parser, TK_JOIN)) {
    if (consume(parser, TK_IDENTIFIER, "Expect join table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    strncpy(stmt->join_table_name, parser->previous_token.text, MAX_TABLE_NAME);
    
    if (consume(parser, TK_ON, "Expect 'ON' after JOIN table") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    
    // Simple ON clause: t1.col = t2.col
    if (consume(parser, TK_IDENTIFIER, "Expect first column in ON") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    strncpy(stmt->join_on_column, parser->previous_token.text, MAX_COLUMN_NAME);
    
    if (consume(parser, TK_EQUALS, "Expect '=' in ON clause") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    
    if (consume(parser, TK_IDENTIFIER, "Expect second column in ON") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    strncpy(stmt->join_with_column, parser->previous_token.text, MAX_COLUMN_NAME);
  }
  
  return parse_where(parser, &table->schema, &stmt->where_clause);
}

static PrepareResult parse_delete(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_DELETE;
  DeleteStatement* stmt = &statement->delete;
  
  if (consume(parser, TK_FROM, "Expect 'FROM' after 'DELETE'") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  
  Table* table = catalog_get_table(parser->catalog, stmt->table_name);
  if (table == NULL) {
    printf("Error: Table '%s' does not exist\n", stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  return parse_where(parser, &table->schema, &stmt->where_clause);
}

static PrepareResult parse_create_index(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_CREATE_INDEX;
  CreateIndexStatement* stmt = &statement->create_index;
  
  if (consume(parser, TK_IDENTIFIER, "Expect index name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  strncpy(stmt->index_name, parser->previous_token.text, MAX_INDEX_NAME);
  
  if (consume(parser, TK_ON, "Expect 'ON' after index name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  
  Table* table = catalog_get_table(parser->catalog, stmt->table_name);
  if (table == NULL) {
    printf("Error: Table '%s' does not exist\n", stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  if (consume(parser, TK_LPAREN, "Expect '(' before column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  if (consume(parser, TK_IDENTIFIER, "Expect column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  strncpy(stmt->column_name, parser->previous_token.text, MAX_COLUMN_NAME);
  
  bool found = false;
  for (uint32_t i = 0; i < table->schema.num_columns; i++) {
    if (strcasecmp(table->schema.columns[i].name, stmt->column_name) == 0) {
      found = true;
      break;
    }
  }
  if (!found) {
    printf("Error: Column '%s' not found in table '%s'\n", stmt->column_name, stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  if (consume(parser, TK_RPAREN, "Expect ')' after column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  return PREPARE_SUCCESS;
}

static PrepareResult parse_update(Parser* parser, Statement* statement) {
  statement->type = STATEMENT_UPDATE;
  UpdateStatement* stmt = &statement->update;
  memset(stmt->values, 0, sizeof(stmt->values));
  
  if (consume(parser, TK_IDENTIFIER, "Expect table name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  strncpy(stmt->table_name, parser->previous_token.text, MAX_TABLE_NAME);
  
  Table* table = catalog_get_table(parser->catalog, stmt->table_name);
  if (table == NULL) {
    printf("Error: Table '%s' does not exist\n", stmt->table_name);
    return PREPARE_SYNTAX_ERROR;
  }
  
  if (consume(parser, TK_SET, "Expect 'SET' in UPDATE statement") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
  
  stmt->num_values = table->schema.num_columns;
  stmt->set_mask = 0;
  
  do {
    if (consume(parser, TK_IDENTIFIER, "Expect column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    char col_name[MAX_COLUMN_NAME];
    strncpy(col_name, parser->previous_token.text, MAX_COLUMN_NAME);
    
    int col_idx = -1;
    for (uint32_t i = 0; i < table->schema.num_columns; i++) {
      if (strcasecmp(table->schema.columns[i].name, col_name) == 0) {
        col_idx = (int)i;
        break;
      }
    }
    
    if (col_idx == -1) {
      printf("Error: Column '%s' not found\n", col_name);
      return PREPARE_SYNTAX_ERROR;
    }
    
    if (consume(parser, TK_EQUALS, "Expect '=' after column name") != PREPARE_SUCCESS) return PREPARE_SYNTAX_ERROR;
    
    if (match(parser, TK_INTEGER_LITERAL)) {
      stmt->values[col_idx].int_value = atoi(parser->previous_token.text);
    } else if (match(parser, TK_STRING_LITERAL)) {
      stmt->values[col_idx].string_value = strdup(parser->previous_token.text);
    } else {
      printf("Error: Expect literal value\n");
      return PREPARE_SYNTAX_ERROR;
    }
    stmt->set_mask |= (1 << col_idx);
  } while (match(parser, TK_COMMA));
  
  return parse_where(parser, &table->schema, &stmt->where_clause);
}

PrepareResult parser_parse(Parser* parser, Statement* statement) {
  if (match(parser, TK_CREATE)) {
    if (match(parser, TK_TABLE)) return parse_create_table(parser, statement);
    if (match(parser, TK_INDEX)) return parse_create_index(parser, statement);
    return PREPARE_UNRECOGNIZED_STATEMENT;
  }
  if (match(parser, TK_INSERT)) return parse_insert(parser, statement);
  if (match(parser, TK_SELECT)) return parse_select(parser, statement);
  if (match(parser, TK_UPDATE)) return parse_update(parser, statement);
  if (match(parser, TK_DELETE)) return parse_delete(parser, statement);
  
  return PREPARE_UNRECOGNIZED_STATEMENT;
}



