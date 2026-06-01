#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"
#include "statement.h"
#include "table.h"

typedef struct {
  Tokenizer tokenizer;
  Token current_token;
  Token previous_token;
  Catalog* catalog;
  bool had_error;
} Parser;

void parser_init(Parser* parser, const char* source, Catalog* catalog);
PrepareResult parser_parse(Parser* parser, Statement* statement);

#endif
