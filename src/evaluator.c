#include "evaluator.h"
#include <string.h>

static void* get_column_value(void* record_data, TableSchema* schema, uint32_t col_idx) {
  uint32_t offset = 0;
  for (uint32_t i = 0; i < col_idx; i++) {
    if (schema->columns[i].type == TYPE_INT) {
      offset += sizeof(int32_t);
    } else {
      char* val = (char*)(record_data + offset);
      offset += strlen(val) + 1;
    }
  }
  return record_data + offset;
}

bool evaluate(Expression* expr, void* record_data, TableSchema* schema) {
  if (expr == NULL) return true;
  
  if (expr->type == EXPR_COLUMN) {
    // This case shouldn't be called directly as the top-level evaluate
    // but rather inside binary op. But for robustness:
    return false; 
  }
  
  if (expr->type == EXPR_LITERAL) {
    return false; // Literals aren't boolean truthy alone in this DB
  }
  
  if (expr->type == EXPR_BINARY_OP) {
    if (expr->binary.op == OP_AND) {
      return evaluate(expr->binary.left, record_data, schema) && evaluate(expr->binary.right, record_data, schema);
    }
    if (expr->binary.op == OP_OR) {
      return evaluate(expr->binary.left, record_data, schema) || evaluate(expr->binary.right, record_data, schema);
    }
    
    // Comparison operators
    Expression* left = expr->binary.left;
    Expression* right = expr->binary.right;
    
    // Extract values
    int32_t left_int, right_int;
    char *left_str, *right_str;
    DataType type;
    
    if (left->type == EXPR_COLUMN) {
      type = schema->columns[left->column.column_idx].type;
      void* val_ptr = get_column_value(record_data, schema, left->column.column_idx);
      if (type == TYPE_INT) memcpy(&left_int, val_ptr, sizeof(int32_t));
      else left_str = (char*)val_ptr;
    } else {
      type = left->literal.type;
      if (type == TYPE_INT) left_int = left->literal.int_value;
      else left_str = left->literal.string_value;
    }
    
    if (right->type == EXPR_COLUMN) {
      void* val_ptr = get_column_value(record_data, schema, right->column.column_idx);
      if (type == TYPE_INT) memcpy(&right_int, val_ptr, sizeof(int32_t));
      else right_str = (char*)val_ptr;
    } else {
      if (type == TYPE_INT) right_int = right->literal.int_value;
      else right_str = right->literal.string_value;
    }
    
    // Perform comparison
    if (type == TYPE_INT) {
      switch (expr->binary.op) {
        case OP_EQ: return left_int == right_int;
        case OP_NE: return left_int != right_int;
        case OP_GT: return left_int > right_int;
        case OP_LT: return left_int < right_int;
        case OP_GE: return left_int >= right_int;
        case OP_LE: return left_int <= right_int;
        default: return false;
      }
    } else {
      int cmp = strcmp(left_str, right_str);
      switch (expr->binary.op) {
        case OP_EQ: return cmp == 0;
        case OP_NE: return cmp != 0;
        case OP_GT: return cmp > 0;
        case OP_LT: return cmp < 0;
        case OP_GE: return cmp >= 0;
        case OP_LE: return cmp <= 0;
        default: return false;
      }
    }
  }
  
  return false;
}
