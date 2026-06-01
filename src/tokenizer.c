#include "tokenizer.h"
#include <ctype.h>

void tokenizer_init(Tokenizer* tokenizer, const char* source) {
  tokenizer->source = source;
  tokenizer->current = 0;
}

static bool is_at_end(Tokenizer* tokenizer) {
  return tokenizer->source[tokenizer->current] == '\0';
}

static char peek(Tokenizer* tokenizer) {
  return tokenizer->source[tokenizer->current];
}

static char advance(Tokenizer* tokenizer) {
  return tokenizer->source[tokenizer->current++];
}

static Token make_token(TokenType type, const char* start, uint32_t length) {
  Token token;
  token.type = type;
  token.text = malloc(length + 1);
  memcpy(token.text, start, length);
  token.text[length] = '\0';
  token.length = length;
  return token;
}

static void skip_whitespace(Tokenizer* tokenizer) {
  while (!is_at_end(tokenizer) && isspace(peek(tokenizer))) {
    advance(tokenizer);
  }
}

static char peek_next(Tokenizer* tokenizer) {
  if (is_at_end(tokenizer)) return '\0';
  return tokenizer->source[tokenizer->current + 1];
}

static TokenType check_keyword(const char* start, uint32_t length) {
  if (strncasecmp(start, "CREATE", length) == 0 && length == 6) return TK_CREATE;
  if (strncasecmp(start, "TABLE", length) == 0 && length == 5) return TK_TABLE;
  if (strncasecmp(start, "INDEX", length) == 0 && length == 5) return TK_INDEX;
  if (strncasecmp(start, "ON", length) == 0 && length == 2) return TK_ON;
  if (strncasecmp(start, "INSERT", length) == 0 && length == 6) return TK_INSERT;

  if (strncasecmp(start, "INTO", length) == 0 && length == 4) return TK_INTO;
  if (strncasecmp(start, "VALUES", length) == 0 && length == 6) return TK_VALUES;
  if (strncasecmp(start, "SELECT", length) == 0 && length == 6) return TK_SELECT;
  if (strncasecmp(start, "UPDATE", length) == 0 && length == 6) return TK_UPDATE;
  if (strncasecmp(start, "DELETE", length) == 0 && length == 6) return TK_DELETE;
  if (strncasecmp(start, "SET", length) == 0 && length == 3) return TK_SET;
  if (strncasecmp(start, "JOIN", length) == 0 && length == 4) return TK_JOIN;
  if (strncasecmp(start, "FROM", length) == 0 && length == 4) return TK_FROM;

  if (strncasecmp(start, "INT", length) == 0 && length == 3) return TK_INT;
  if (strncasecmp(start, "TEXT", length) == 0 && length == 4) return TK_TEXT;
  if (strncasecmp(start, "WHERE", length) == 0 && length == 5) return TK_WHERE;
  if (strncasecmp(start, "AND", length) == 0 && length == 3) return TK_AND;
  if (strncasecmp(start, "OR", length) == 0 && length == 2) return TK_OR;
  return TK_IDENTIFIER;
}


static Token identifier(Tokenizer* tokenizer) {
  const char* start = tokenizer->source + tokenizer->current;
  while (!is_at_end(tokenizer) && (isalnum(peek(tokenizer)) || peek(tokenizer) == '_')) {
    advance(tokenizer);
  }
  uint32_t length = (tokenizer->source + tokenizer->current) - start;
  TokenType type = check_keyword(start, length);
  return make_token(type, start, length);
}

static Token number(Tokenizer* tokenizer) {
  const char* start = tokenizer->source + tokenizer->current;
  while (!is_at_end(tokenizer) && isdigit(peek(tokenizer))) {
    advance(tokenizer);
  }
  uint32_t length = (tokenizer->source + tokenizer->current) - start;
  return make_token(TK_INTEGER_LITERAL, start, length);
}

static Token string(Tokenizer* tokenizer) {
  advance(tokenizer); // Skip opening quote
  const char* start = tokenizer->source + tokenizer->current;
  while (!is_at_end(tokenizer) && peek(tokenizer) != '\'') {
    advance(tokenizer);
  }
  
  uint32_t length = (tokenizer->source + tokenizer->current) - start;
  
  if (is_at_end(tokenizer)) {
    // Unterminated string
    return make_token(TK_UNKNOWN, start, length);
  }
  
  advance(tokenizer); // Skip closing quote
  return make_token(TK_STRING_LITERAL, start, length);
}

Token tokenizer_next_token(Tokenizer* tokenizer) {
  skip_whitespace(tokenizer);
  
  if (is_at_end(tokenizer)) {
    return make_token(TK_EOF, "", 0);
  }
  
  char c = peek(tokenizer);
  
  if (isalpha(c) || c == '_') return identifier(tokenizer);
  if (isdigit(c)) return number(tokenizer);
  if (c == '\'') return string(tokenizer);
  
  advance(tokenizer);
  switch (c) {
    case '*': return make_token(TK_STAR, "*", 1);
    case ',': return make_token(TK_COMMA, ",", 1);
    case '(': return make_token(TK_LPAREN, "(", 1);
    case ')': return make_token(TK_RPAREN, ")", 1);
    case '=': return make_token(TK_EQUALS, "=", 1);
    case '>':
      if (peek(tokenizer) == '=') {
        advance(tokenizer);
        return make_token(TK_GE, ">=", 2);
      }
      return make_token(TK_GT, ">", 1);
    case '<':
      if (peek(tokenizer) == '=') {
        advance(tokenizer);
        return make_token(TK_LE, "<=", 2);
      }
      return make_token(TK_LT, "<", 1);
    case '!':
      if (peek(tokenizer) == '=') {
        advance(tokenizer);
        return make_token(TK_BANG_EQUALS, "!=", 2);
      }
      return make_token(TK_UNKNOWN, "!", 1);
  }

  
  return make_token(TK_UNKNOWN, tokenizer->source + tokenizer->current - 1, 1);
}

void token_free(Token* token) {
  if (token->text) {
    free(token->text);
    token->text = NULL;
  }
}

const char* token_type_to_string(TokenType type) {
  switch (type) {
    case TK_CREATE: return "CREATE";
    case TK_TABLE: return "TABLE";
    case TK_INDEX: return "INDEX";
    case TK_ON: return "ON";
    case TK_INSERT: return "INSERT";

    case TK_INTO: return "INTO";
    case TK_VALUES: return "VALUES";
    case TK_SELECT: return "SELECT";
    case TK_UPDATE: return "UPDATE";
    case TK_DELETE: return "DELETE";
    case TK_SET: return "SET";
    case TK_FROM: return "FROM";

    case TK_INT: return "INT";
    case TK_TEXT: return "TEXT";
    case TK_WHERE: return "WHERE";
    case TK_AND: return "AND";
    case TK_OR: return "OR";
    case TK_STAR: return "*";
    case TK_COMMA: return ",";
    case TK_LPAREN: return "(";
    case TK_RPAREN: return ")";
    case TK_EQUALS: return "=";
    case TK_GT: return ">";
    case TK_LT: return "<";
    case TK_GE: return ">=";
    case TK_LE: return "<=";
    case TK_BANG_EQUALS: return "!=";
    case TK_IDENTIFIER: return "IDENTIFIER";

    case TK_INTEGER_LITERAL: return "INTEGER_LITERAL";
    case TK_STRING_LITERAL: return "STRING_LITERAL";
    case TK_EOF: return "EOF";
    case TK_UNKNOWN: return "UNKNOWN";
  }
  return "???";
}
