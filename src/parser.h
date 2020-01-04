#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdarg.h>

#include "lexer.h"
#include "stretchy_buffer.h"

typedef enum node_type {
  NODE_TYPE_TOP_LEVEL, // better naming?
  NODE_TYPE_INSTRUCTION,
  NODE_TYPE_LABEL,
  NODE_TYPE_OPERAND_SIMPLE, // number, register, symbol;  better naming?
  NODE_TYPE_OPERAND_MEM_ADDRESS,
} node_type;

// TODO: XXX I guess I should use this in the nodes, instead of
// just node_type............
typedef struct node {
  node_type type;
} node;

/*typedef struct operand_node {
  node_type type;
} operand_node;*/

typedef struct operand_simple_node {
  node_type type;
  token* token;
} operand_simple_node;

typedef struct operand_mem_address_node {
  node_type type;
  token* offset_token;
  token* register_token;
  src_pos pos;
} operand_mem_address_node;

typedef struct instruction_node {
  node_type type;
  int num_operands;
  node** operands;
  token* opcode;
} instruction_node;

typedef struct label_node { // TODO: rename?
  node_type type;
  int num_instructions;
  instruction_node** instructions;
  token* name;
} label_node;

typedef struct top_level_node {
  node_type type;

  instruction_node** instructions;
  int num_instructions;

  label_node** labels;
  int num_labels;
} top_level_node;


typedef struct parser {
  token** tokens;
  int cur_token;
  int num_tokens;
  error_handler* error_handler;
} parser;

static void parser_report_error(parser* p, char* msg, token* tok) {
  assert(p->error_handler);
  assert(p->error_handler->handler_fn);

  p->error_handler->handler_fn(p->error_handler, msg, tok == NULL ? NULL : &tok->pos);
}

static void parser_report_errorf(parser* p, token* tok, const char* fmt, ...) {
  static char msg_buf[1024];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg_buf, 1024, fmt, args);
  va_end(args);

  parser_report_error(p, msg_buf, tok);
}

void parser_init(parser* p, const char* code) {
  lexer lex;
  lexer_init(&lex, code);

  p->error_handler = lex.error_handler;

  // Read all tokens "before parsing"
  token** tokens = NULL;
  token* t;
  while ((t = lexer_next_token(&lex)) != NULL) {
    sb_push(tokens, t);
    if (t->type == TT_EOF) break;
  }

  p->tokens = tokens;
  p->num_tokens = sb_count(tokens);
  p->cur_token = 0;
}

token* parser_next_token(parser* p) {
  if (p->cur_token >= p->num_tokens) {
    return p->tokens[p->num_tokens - 1]; // EOF
  }
  return p->tokens[p->cur_token++];
}

token* parser_peek_token(parser* p) {
  if (p->cur_token >= p->num_tokens) {
    return p->tokens[p->num_tokens - 1]; // EOF
  }
  return p->tokens[p->cur_token];
}

token* parser_next_skip_new_line(parser* p) {
  token* t = parser_next_token(p);
  while (t->type == TT_NEW_LINE) {
    t = parser_next_token(p);
  }
  return t;
}

token* parser_peek_skip_new_line(parser* p) {
  int cur_token = p->cur_token;
  token* t;
  do {
    t = parser_next_token(p);
  } while (t->type == TT_NEW_LINE);
  p->cur_token = cur_token;
  return t;
}

bool is_next_tokens_a_label(parser* p) {
  token* t0 = parser_next_skip_new_line(p);
  token* t1 = parser_next_token(p); // NOTE: we don't allow a new line before ':' in a label...

  p->cur_token -= 2; // 'push back'

  return (t0->type == TT_SYMBOL) && (t1->type == TT_COLON);
}

#define IS_NEW_LINE_OR_EOF(t) (t->type == TT_NEW_LINE || t->type == TT_EOF)

static token* parser_expect_token(parser* p, token_type tt) {
  token* t = parser_next_token(p);
  if (t->type != tt) {
    parser_report_errorf(p, t, "expected a '%s', but got '%s'.",
      friendly_token_type_names[tt], token_value_to_string(t));
  }
  return t;
}

node* parse_operand(parser* p) {
  token* t = parser_next_token(p);

  // parse '[symbol]' or 'number[symbol]'
  if (t->type == TT_BRACKET_OPEN || parser_peek_token(p)->type == TT_BRACKET_OPEN) {
    bool has_offset = t->type != TT_BRACKET_OPEN;

    if (has_offset && t->type != TT_INT) {
      parser_report_errorf(p, t,
        "invalid token '%s' before memory address. Expected an integer as offset.",
        friendly_token_type_names[t->type]);
      return NULL;
    }

    if (has_offset) {
      (void) parser_next_token(p); // Skip [
    }

    token* reg_token = parser_expect_token(p, TT_SYMBOL);
    token* bracket_close = parser_expect_token(p, TT_BRACKET_CLOSE);

    operand_mem_address_node* n = (operand_mem_address_node*)
      malloc(sizeof(operand_mem_address_node));
    n->type = NODE_TYPE_OPERAND_MEM_ADDRESS;
    n->register_token = reg_token;
    n->pos.line_number = t->pos.line_number;
    n->pos.col_start = t->pos.col_start;
    n->pos.col_end = bracket_close->pos.col_end;
    n->offset_token = has_offset ? t : NULL;
    return (node*) n;
  }

  if (t->type == TT_SYMBOL || t->type == TT_STRING || t->type == TT_INT) {
    operand_simple_node* n = (operand_simple_node*) malloc(sizeof(operand_simple_node));
    n->type = NODE_TYPE_OPERAND_SIMPLE;
    n->token = t;
    return (node*) n;
  }

  parser_report_errorf(p, t,
    "unexpected token '%s' as an operand.", token_value_to_string(t));
  return NULL;
}

node** parse_operands(parser* p) {
  node** operands = NULL;

  while (1) {
    node* operand = parse_operand(p);
    sb_push(operands, operand);

    token* t = parser_next_token(p);
    if (IS_NEW_LINE_OR_EOF(t))
      break;

    // Expect ','
    if (t->type != TT_COMMA) {
      parser_report_errorf(p, t, "expected ',' between operands, but got '%s'.",
                           token_value_to_string(t));
      break;
    }
  }

  return operands;
}


instruction_node* parser_parse_instruction(parser* p) {
  token* opcode = parser_next_skip_new_line(p);
  assert(opcode->type == TT_SYMBOL);

  node** operands = NULL;

  token* t = parser_peek_token(p);
  if (!IS_NEW_LINE_OR_EOF(t)) {
    operands = parse_operands(p);
  }

  instruction_node* node = (instruction_node*) malloc(sizeof(instruction_node));
  node->type = NODE_TYPE_INSTRUCTION;
  node->opcode = opcode;
  node->operands = operands;
  node->num_operands = sb_count(operands);
  return node;
}

label_node* parser_parse_label(parser* p) {
  token* name_token = parser_next_skip_new_line(p);
  assert(name_token->type == TT_SYMBOL);

  {
    // We don't actually need to do this because we're already
    // checking if next token is ':' in parse top level...
    token* t = parser_next_token(p);
    assert(t->type == TT_COLON);
  }

  instruction_node** instructions = NULL;

  {
    token* t = parser_peek_skip_new_line(p);
    if (t->type != TT_SYMBOL) {
      parser_report_errorf(p, t, "unexpected token '%s' after a label.",
                           token_value_to_string(t));
      return NULL;
    }

    while (1) {
      instruction_node* in = parser_parse_instruction(p);
      sb_push(instructions, in);

      if (parser_peek_skip_new_line(p)->type == TT_EOF || is_next_tokens_a_label(p))
        break;
    }
  }

  label_node* node = (label_node*) malloc(sizeof(label_node));
  node->type = NODE_TYPE_LABEL;
  node->name = name_token;
  node->instructions = instructions;
  node->num_instructions = sb_count(instructions);
  return node;
}

// parse top lavel instructions or labeled instructions
node* parser_parse_top_level(parser* p) {
  if (is_next_tokens_a_label(p)) {
    return (node*) parser_parse_label(p);
  }

  token* t = parser_peek_skip_new_line(p);
  if (t->type == TT_SYMBOL) {
    return (node*) parser_parse_instruction(p);
  }

  parser_report_errorf(p, t, "unexpected token '%s (%s)' at top level. "
                             "Expected a instruction or a label.",
                             token_type_names[t->type], token_value_to_string(t));
  return NULL;
}

top_level_node* parser_parse(parser* p) {
  label_node** label_nodes = NULL;
  instruction_node** instructions = NULL;

  while (parser_peek_skip_new_line(p)->type != TT_EOF) {
    node* n = parser_parse_top_level(p);
    if (n->type == NODE_TYPE_LABEL) {
      sb_push(label_nodes, (label_node*) n);
    } else if (n->type == NODE_TYPE_INSTRUCTION) {
      sb_push(instructions, (instruction_node*) n);
    } else {
      assert(0);
    }
  }

  top_level_node* n = (top_level_node*) malloc(sizeof(top_level_node));
  n->type = NODE_TYPE_TOP_LEVEL;
  n->instructions = instructions;
  n->num_instructions = sb_count(instructions);
  n->labels = label_nodes;
  n->num_labels= sb_count(label_nodes);
  return n;
}

#endif // __PARSER_H__