#ifndef __LEXER_H__
#define __LEXER_H__

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#include "error.h"

typedef enum token_type {
  TT_SYMBOL,
  TT_STRING,
  TT_INT,
  TT_COLON,
  TT_COMMA,
  TT_BRACKET_OPEN,
  TT_BRACKET_CLOSE,
  TT_NEW_LINE,
  TT_EOF,
} token_type;

const char* token_type_names[] = {
  "SYMBOL",
  "STRING",
  "INT",
  "COLON",
  "COMMA",
  "BRACKET_OPEN",
  "BRACKET_CLOSE",
  "NEW_LINE",
  "EOF",
};

const char* friendly_token_type_names[] = {
  "symbol",
  "string",
  "integer",
  ":",
  ",",
  "[",
  "]",
  "new line",
  "eof",
};

typedef struct token {
  token_type type;
  src_pos pos;
  union {
    char* s;
    int64_t i;
    char c;
  };
} token;

typedef struct lexer {
  const char* code;
  int code_len;
  int offset;
  int line_number;
  int column;
  error_handler* error_handler;
} lexer;

void lexer_report_error_pos(lexer* l, src_pos pos, char* msg) {
  assert(l->error_handler);
  assert(l->error_handler->handler_fn);
  l->error_handler->handler_fn(l->error_handler, msg, &pos);
}

void lexer_report_error_posf(lexer* l, src_pos pos, char* fmt, ...) {
  static char msg_buf[1024];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg_buf, 1024, fmt, args);
  va_end(args);

  lexer_report_error_pos(l, pos, msg_buf);
}

void lexer_report_error(lexer* l, char* msg) {
  src_pos pos = {
    .line_number = l->line_number,
    .col_start = l->column - 1,
    .col_end = l->column
  };
  lexer_report_error_pos(l, pos, msg);
}

void lexer_init(lexer* l, const char* code) {
  l->code = code;
  l->code_len = strlen(code);
  l->offset = 0;
  l->line_number = 1;
  l->column = 0;
  l->error_handler = default_error_handler(code);
}

token* token_new_pos(token_type type, int line_number, int col_start, int col_end) {
  token* tok = (token*) malloc(sizeof(token));
  tok->type = type;
  tok->s = NULL;
  tok->i = tok->c = 0;
  tok->pos.line_number = line_number;
  tok->pos.col_start = col_start;
  tok->pos.col_end= col_end;
  return tok;
}

token* token_new(token_type type) {
  return token_new_pos(type, -1, -1, -1);
}

// TOOD: instead of copying, can we just provide the ranges?
static char* copy_str_from_code(lexer* l, int offset, int len) {
  char* str = (char*) malloc(len + 1);
  memcpy(str, l->code + offset, len);
  str[len] = '\0';
  return str;
}

bool lexer_eof(lexer* l) {
  return l->offset >= l->code_len;
}

char lexer_peek(lexer* l) {
  if (l->offset >= l->code_len) return EOF;
  return l->code[l->offset];
}

char lexer_next(lexer* l) {
  if (l->offset >= l->code_len) return EOF;
  l->column++;
  return l->code[l->offset++];
}

static inline void lexer_skip(lexer* l) {
  (void) lexer_next(l);
}

static char skip_until_char(lexer* l, char c) {
  while (lexer_peek(l) != c && !lexer_eof(l)) {
    lexer_skip(l);
  }
  return lexer_peek(l);
}

static char skip_until_char2(lexer* l, char c0, char c1) {
  while (lexer_peek(l) != c0 && lexer_peek(l) != c1 && !lexer_eof(l)) {
    lexer_skip(l);
  }
  return lexer_peek(l);
}

static void lexer_skip_comment(lexer* l) {
  skip_until_char(l, '\n');
}

static inline int is_tab_or_space(char c) {
  return c == ' ' || c == '\t';
}

static void skip_spaces(lexer* l) {
  while (!lexer_eof(l) && is_tab_or_space(lexer_peek(l)))
    lexer_skip(l);
}

char lexer_peek_non_space(lexer* l) {
  skip_spaces(l);
  return lexer_eof(l) ? EOF : l->code[l->offset];
}

static token* lex_int(lexer* l) {
  // XXX TODO better parsing & error reporting
  int64_t num;
  int num_length;
  sscanf(l->code + l->offset, "%I64d%n", &num, &num_length);
  l->offset += num_length;
  l->column += num_length;

  token* t = token_new_pos(TT_INT, l->line_number, l->column-num_length, l->column);
  t->i = num;
  return t;
}

static token* lex_string(lexer* l) {
  lexer_skip(l); // opening '
  int start = l->offset;
  { // Find the closing quote
    char c = skip_until_char2(l, '\'', '\n');
    if (c == EOF || c == '\n') {
      int str_len = l->offset - start;
      src_pos pos = {
        .line_number = l->line_number,
        .col_start = l->column - str_len - 1, /* -1 = include opening ' */
        .col_end = l->column - 1
      };
      lexer_report_error_pos(l, pos, "unclosed string literal");
      return NULL;
    }
  }
  int str_len = l->offset - start;
  lexer_skip(l); // closing '

  token* t = token_new_pos(TT_STRING, l->line_number, l->column-str_len-2, l->column);
  t->s = str_len == 0 ? "" : copy_str_from_code(l, start, str_len);
  return t;
}

static token* lex_single_char(lexer* l, token_type type) {
  token* t = token_new_pos(type, l->line_number, l->column, l->column + 1);
  t->c = lexer_next(l);
  return t;
}

static token* lex_symbol(lexer* l) {
  int start = l->offset;

  while ((isalnum(lexer_peek(l)) || lexer_peek(l) == '_') && !lexer_eof(l)) {
    lexer_skip(l);
  }

  int sym_len = l->offset - start;

  token* t = token_new_pos(TT_SYMBOL, l->line_number, l->column - sym_len, l->column);
  t->s = copy_str_from_code(l, start, sym_len);
  return t;
}

// TODO: if the next token 'has an error' it returns NULL, but
// the subsequent calls to lexer_next_tokens works. Should we always return
// NULL if an error ocurred?
token* lexer_next_token(lexer* l) {
  static token token_eof = { .type = TT_EOF };

  char c = lexer_peek_non_space(l);

  // TODO: switch?

  if (c == ';') {
    lexer_skip_comment(l);
    return lexer_next_token(l);
  }

  if (c == '\r') {
    l->offset++;
    return lexer_next_token(l);
  }

  if (isdigit(c)) {
    return lex_int(l);
  }

  if (c == '-' || c == '+') {
    lexer_skip(l);
    if (isdigit(lexer_peek(l))) {
      l->offset--; // push back
      l->column--; // push back
      return lex_int(l);
    }
    lexer_report_error(l, "unexpected token");
    return NULL;
  }

  if (c == '\'') {
    return lex_string(l);
  }

  if (c == '\n') {
    lexer_skip(l);
    token* t = token_new_pos(TT_NEW_LINE, l->line_number, l->column-1, l->column);
    l->line_number++;
    l->column = 0;
    return t;
  }

  #define SINGLE_CHAR(ch, type) \
    if (c == ch) {  \
      return lex_single_char(l, type); \
    }

  SINGLE_CHAR(':', TT_COLON);
  SINGLE_CHAR(',', TT_COMMA);
  SINGLE_CHAR('[', TT_BRACKET_OPEN);
  SINGLE_CHAR(']', TT_BRACKET_CLOSE);

  if (isalpha(c) || c == '_') {
    return lex_symbol(l);
  }

  if (c == EOF) {
    return &token_eof;
  }

  src_pos pos = {
    .line_number = l->line_number,
    .col_start = l->column,
    .col_end = l->column + 1
  };
  lexer_report_error_posf(l, pos, "unexpected char '%c' (%d)", isprint(c) ? c : '?', c);
  return NULL;
}

void print_token_type(token* t) {
  printf("%s", token_type_names[t->type]);
}

// TODO: test?
char* token_value_to_string(token* t) {
  #define BUF_LEN 1024
  static char token_value_buf[BUF_LEN];

  switch (t->type) {
    case TT_SYMBOL:
      snprintf(token_value_buf, BUF_LEN, "%s", t->s);
      break;
    case TT_STRING:
      snprintf(token_value_buf, BUF_LEN, "\'%s\'", t->s);
      break;
    case TT_INT:
      snprintf(token_value_buf, BUF_LEN, "%I64d", t->i);
      break;
    case TT_COLON: case TT_COMMA: case TT_BRACKET_OPEN:
    case TT_BRACKET_CLOSE:
      snprintf(token_value_buf, BUF_LEN, "%c", t->c);
      break;
    case TT_NEW_LINE:
      snprintf(token_value_buf, BUF_LEN, "<NEW_LINE>");
      break;
    case TT_EOF:
      snprintf(token_value_buf, BUF_LEN, "<EOF>");
      break;
    default:
      assert(0);
      break;
  }
  return token_value_buf;
}

void print_token(token* t) {
  printf("{ ");
  print_token_type(t);
  switch (t->type) {
    case TT_SYMBOL:
    case TT_STRING:
      printf(", s='%s', ", t->s);
      break;
    case TT_INT:
      printf(", i=%I64d, ", t->i);
      break;
    case TT_COLON: case TT_COMMA: case TT_BRACKET_OPEN:
    case TT_BRACKET_CLOSE:
      if (t->c == '\n')
        printf(", c='\\n', ");
      else
        printf(", c='%c', ", t->c);
      break;
    default:
      printf(",");
      break;
  }
  printf(" ln=%d  [%d, %d] }\n", t->pos.line_number, t->pos.col_start, t->pos.col_end);
}
#endif // __LEXER_H__