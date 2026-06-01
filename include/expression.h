#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "common.h"
#include "schema.h"

typedef enum {
  EXPR_BINARY_OP,
  EXPR_COLUMN,
  EXPR_LITERAL
} ExpressionType;

typedef enum {
  OP_EQ,
  OP_GT,
  OP_LT,
  OP_GE,
  OP_LE,
  OP_NE,
  OP_AND,
  OP_OR
} Operator;

typedef struct Expression_t {
  ExpressionType type;
  union {
    struct {
      struct Expression_t* left;
      struct Expression_t* right;
      Operator op;
    } binary;
    struct {
      char column_name[MAX_COLUMN_NAME];
      uint32_t column_idx;
    } column;
    struct {
      DataType type;
      union {
        int32_t int_value;
        char* string_value;
      };
    } literal;
  };
} Expression;

Expression* expr_new_binary(Expression* left, Operator op, Expression* right);
Expression* expr_new_column(const char* name, uint32_t idx);
Expression* expr_new_int_literal(int32_t value);
Expression* expr_new_string_literal(const char* value);
void expr_free(Expression* expr);

#endif
