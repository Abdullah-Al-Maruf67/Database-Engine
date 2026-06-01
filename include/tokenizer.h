#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

typedef enum {
  TK_CREATE,
  TK_TABLE,
  TK_INDEX,
  TK_ON,
  TK_INSERT,

  TK_INTO,
  TK_VALUES,
  TK_SELECT,
  TK_UPDATE,
  TK_DELETE,
  TK_SET,
  TK_JOIN,
  TK_FROM,

  TK_INT,
  TK_TEXT,
  TK_WHERE,
  TK_AND,
  TK_OR,
  TK_STAR,
  TK_COMMA,
  TK_LPAREN,
  TK_RPAREN,
  TK_EQUALS,
  TK_GT,
  TK_LT,
  TK_GE,
  TK_LE,
  TK_BANG_EQUALS,
  TK_IDENTIFIER,

  TK_INTEGER_LITERAL,
  TK_STRING_LITERAL,
  TK_EOF,
  TK_UNKNOWN
} TokenType;

typedef struct {
  TokenType type;
  char* text;
  uint32_t length;
} Token;

typedef struct {
  const char* source;
  uint32_t current;
} Tokenizer;

void tokenizer_init(Tokenizer* tokenizer, const char* source);
Token tokenizer_next_token(Tokenizer* tokenizer);
void token_free(Token* token);
const char* token_type_to_string(TokenType type);

#endif
