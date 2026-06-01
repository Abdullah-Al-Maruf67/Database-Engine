#include "expression.h"

Expression* expr_new_binary(Expression* left, Operator op, Expression* right) {
  Expression* expr = malloc(sizeof(Expression));
  expr->type = EXPR_BINARY_OP;
  expr->binary.left = left;
  expr->binary.op = op;
  expr->binary.right = right;
  return expr;
}

Expression* expr_new_column(const char* name, uint32_t idx) {
  Expression* expr = malloc(sizeof(Expression));
  expr->type = EXPR_COLUMN;
  strncpy(expr->column.column_name, name, MAX_COLUMN_NAME);
  expr->column.column_idx = idx;
  return expr;
}

Expression* expr_new_int_literal(int32_t value) {
  Expression* expr = malloc(sizeof(Expression));
  expr->type = EXPR_LITERAL;
  expr->literal.type = TYPE_INT;
  expr->literal.int_value = value;
  return expr;
}

Expression* expr_new_string_literal(const char* value) {
  Expression* expr = malloc(sizeof(Expression));
  expr->type = EXPR_LITERAL;
  expr->literal.type = TYPE_TEXT;
  expr->literal.string_value = strdup(value);
  return expr;
}

void expr_free(Expression* expr) {
  if (expr == NULL) return;
  if (expr->type == EXPR_BINARY_OP) {
    expr_free(expr->binary.left);
    expr_free(expr->binary.right);
  } else if (expr->type == EXPR_LITERAL && expr->literal.type == TYPE_TEXT) {
    free(expr->literal.string_value);
  }
  free(expr);
}
