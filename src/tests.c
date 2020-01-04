#define TEST_USE_ANSI_COLOR

#include "alloc_spy.h"

#include "test.h"
#include "lexer.h"
#include "parser.h"
#include "interp.h"

// TODO: free memory?
// TODO: add more error tests

// Error handler that "returns" the error message and the position
void test_error_handler(error_handler* err_handler, char* msg, src_pos* pos) {
  void** custom_data = err_handler->handler_data;
  assert(custom_data);

  char** out_msg = (char**) custom_data[0];
  src_pos* out_pos = (src_pos*) custom_data[1];

  *out_msg = (char*) msg;

  out_pos->line_number = pos->line_number;
  out_pos->col_start = pos->col_start;
  out_pos->col_end = pos->col_end;
}

#define ASSERT_POS(got_pos, ln, cend, cstart) \
  ASSERT_EQI(got_pos.line_number, ln);  \
  ASSERT_EQI(got_pos.col_start, cend);  \
  ASSERT_EQI(got_pos.col_end, cstart);

DEF_TEST(parser_errors)
  // TODO: more errors
  src_pos out_pos;
  char* out_msg = NULL;

  void** data = malloc(sizeof(void*) * 2);
  data[0] = &out_msg;
  data[1] = &out_pos;

  parser parser;
  parser_init(&parser, "123");
  parser.error_handler = &(error_handler) {
    .handler_fn = test_error_handler,
    .handler_data = data
  };

  node* n = parser_parse_top_level(&parser);
  if (n != NULL) {
    FAIL("Error not triggered");
  }

  ASSERT_EQS(out_msg, "unexpected token 'INT (123)' at top level. "
                      "Expected a instruction or a label.");
  ASSERT_POS(out_pos, 1, 0, 3);
  free(data);
END_TEST

DEF_TEST(lexer_error_unclosed_string)
  const char* tests[] = {
    "'bar  , 5",

    "msg 5, a, 3, 'foo",

    // -- multi line
    "mov a, b\n"
    "mov c, d\n"
    "msg 'c=', c, 'd=, d\n"
    "sub a, 1\n"
    "end\n",
    // --
  };
  const src_pos tests_pos[] = {
    {.line_number = 1, .col_start = 0, .col_end = 8},
    {.line_number = 1, .col_start = 13, .col_end = 16},
    {.line_number = 3, .col_start = 13, .col_end = 18},
  };

  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    lexer l;
    lexer_init(&l, tests[i]);

    src_pos out_pos;
    char* out_err_msg;

    void** custom_data = malloc(2 * sizeof(void*));
    custom_data[0] = &out_err_msg;
    custom_data[1] = &out_pos;

    error_handler err_handler = {
      .handler_fn = test_error_handler,
      .handler_data = custom_data
    };

    l.error_handler = &err_handler;

    token* t;
    do {
      t = lexer_next_token(&l);
    } while (t != NULL && t->type != TT_EOF);

    if (t != NULL) {
      FAILF("Test %d did not triggered an error", i);
    }

    src_pos epos = tests_pos[i];

    ASSERT_EQS(out_err_msg, "unclosed string literal");
    ASSERT_POS(out_pos, epos.line_number, epos.col_start, epos.col_end);
    free(custom_data);
  }
END_TEST

DEF_TEST(lexer_eof)
  lexer l;
  lexer_init(&l, "");

  ASSERT_EQ(lexer_eof(&l), 1);
END_TEST

DEF_TEST(lexer_positions)
  lexer l;
  lexer_init(&l,
    /* 1  */ "; My first program\n"
    /* 2  */ "mov  a, 123\n"
    /* 3  */ "inc  a\n"
    /* 4  */ "call function\n"
    /* 5  */ "msg  '(5+1)/2 = ', a    ; output message\n"
    /* 6  */ "end\n"
    /* 7  */ "\n"
    /* 8  */ "function:\n"
    /* 9  */ "    div  a, 2\n"
    /* 10 */ "    ret\n"
    /* 11 */ "foo:\n"
    /* 12 */ " mov a, b\n"
    /* 13 */ " mov b, a\n"
    /* 14 */ " ret\n"
    /* 15 */ "\n"
  );

  #if 0
  token* t;
  do {
    t = lexer_next_token(&l);
    printf("{ %d, %d, %d }, // ", t->line_number, t->col_start, t->col_end);
    print_token_type(t);
    printf("\n");
  } while (t->type != TT_EOF);
  #endif

  // TODO: update TT_MISC
  // line_number, col_start, col_end
  int positions[][3] = {
    { 1, 18, 19 }, // TT_NEW_LINE
    { 2, 0, 3 }, // TT_SYMBOL
    { 2, 5, 6 }, // TT_SYMBOL
    { 2, 6, 7 }, // TT_MISC
    { 2, 8, 11 }, // TT_INT
    { 2, 11, 12 }, // TT_NEW_LINE
    { 3, 0, 3 }, // TT_SYMBOL
    { 3, 5, 6 }, // TT_SYMBOL
    { 3, 6, 7 }, // TT_NEW_LINE
    { 4, 0, 4 }, // TT_SYMBOL
    { 4, 5, 13 }, // TT_SYMBOL
    { 4, 13, 14 }, // TT_NEW_LINE
    { 5, 0, 3 }, // TT_SYMBOL
    { 5, 5, 17 }, // TT_STRING
    { 5, 17, 18 }, // TT_MISC
    { 5, 19, 20 }, // TT_SYMBOL
    { 5, 40, 41 }, // TT_NEW_LINE
    { 6, 0, 3 }, // TT_SYMBOL
    { 6, 3, 4 }, // TT_NEW_LINE
    { 7, 0, 1 }, // TT_NEW_LINE
    { 8, 0, 8 }, // TT_SYMBOL
    { 8, 8, 9 }, // TT_MISC
    { 8, 9, 10 }, // TT_NEW_LINE
    { 9, 4, 7 }, // TT_SYMBOL
    { 9, 9, 10 }, // TT_SYMBOL
    { 9, 10, 11 }, // TT_MISC
    { 9, 12, 13 }, // TT_INT
    { 9, 13, 14 }, // TT_NEW_LINE
    { 10, 4, 7 }, // TT_SYMBOL
    { 10, 7, 8 }, // TT_NEW_LINE
    { 11, 0, 3 }, // TT_SYMBOL
    { 11, 3, 4 }, // TT_MISC
    { 11, 4, 5 }, // TT_NEW_LINE
    { 12, 1, 4 }, // TT_SYMBOL
    { 12, 5, 6 }, // TT_SYMBOL
    { 12, 6, 7 }, // TT_MISC
    { 12, 8, 9 }, // TT_SYMBOL
    { 12, 9, 10 }, // TT_NEW_LINE
    { 13, 1, 4 }, // TT_SYMBOL
    { 13, 5, 6 }, // TT_SYMBOL
    { 13, 6, 7 }, // TT_MISC
    { 13, 8, 9 }, // TT_SYMBOL
    { 13, 9, 10 }, // TT_NEW_LINE
    { 14, 1, 4 }, // TT_SYMBOL
    { 14, 4, 5 }, // TT_NEW_LINE
    { 15, 0, 1 }, // TT_NEW_LINE
    { 0, 0, 0 }, // TT_EOF
  };

  for (int i = 0; i < sizeof(positions)/sizeof(positions[0]); i++) {
    token* t = lexer_next_token(&l);
    ASSERT_EQI(t->pos.line_number, positions[i][0]);
    ASSERT_EQI(t->pos.col_start, positions[i][1]);
    ASSERT_EQI(t->pos.col_end, positions[i][2]);
  }
END_TEST

DEF_TEST(lexer_peek_next)
  lexer l;
  lexer_init(&l, "  foo");

  ASSERT_EQC(lexer_peek_non_space(&l), 'f');

  ASSERT_EQC(lexer_next(&l), 'f');
  ASSERT_EQC(lexer_next(&l), 'o');
  ASSERT_EQC(lexer_next(&l), 'o');

  ASSERT_EQC(lexer_next(&l), EOF);
  ASSERT(lexer_eof(&l));
END_TEST

DEF_TEST(lexer_int_token)
  lexer l;
  lexer_init(&l, "-223 100");

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_INT);
    ASSERT_EQI(t->i, -223);
  }

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_INT);
    ASSERT_EQI(t->i, 100);
  }

END_TEST

DEF_TEST(lexer_str_token)
  lexer l;
  lexer_init(&l, "'(5+1)/2 = ' '' 'foo'");

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_STRING);
    ASSERT_EQS(t->s, "(5+1)/2 = ");
  }
  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_STRING);
    ASSERT_EQS(t->s, "");
  }
  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_STRING);
    ASSERT_EQS(t->s, "foo");
  }
END_TEST

DEF_TEST(lexer_misc_token)
  lexer l;
  lexer_init(&l, ",  :");

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_COMMA);
    ASSERT_EQC(t->c, ',');
  }

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_COLON);
    ASSERT_EQC(t->c, ':');
  }
END_TEST

DEF_TEST(lexer_multiple)
  lexer l;
  lexer_init(&l, "msg  '(5+1)/2 = ', a    ; output message\n");

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_NOT_NULL(t->s);
    ASSERT_EQI(t->type, TT_SYMBOL);
    ASSERT_EQS(t->s, "msg");
  }

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_NOT_NULL(t->s);
    ASSERT_EQI(t->type, TT_STRING);
    ASSERT_EQS(t->s, "(5+1)/2 = ");
  }

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_COMMA);
    ASSERT_EQC(t->c, ',');
  }
  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_NOT_NULL(t->s);
    ASSERT_EQI(t->type, TT_SYMBOL);
    ASSERT_EQS(t->s, "a");
  }

  {
    token* t = lexer_next_token(&l);
    ASSERT_NOT_NULL(t);
    ASSERT_EQI(t->type, TT_NEW_LINE);
  }

  ASSERT(lexer_eof(&l));
END_TEST

DEF_TEST(parse_instruction_without_operands)
  parser p;
  parser_init(&p, "ret");

  {
    instruction_node* n = parser_parse_instruction(&p);
    ASSERT_NOT_NULL(n);
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);

    ASSERT_NOT_NULL(n->opcode);
    ASSERT_EQS(n->opcode->s, "ret");

    ASSERT_EQI(n->num_operands, 0);
  }
END_TEST

DEF_TEST(parse_instruction_with_operands)
  parser p;
  parser_init(&p, "mov a, 5");

  {
    instruction_node* n = parser_parse_instruction(&p);
    ASSERT_NOT_NULL(n);
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);

    ASSERT_NOT_NULL(n->opcode);
    ASSERT_EQS(n->opcode->s, "mov");

    ASSERT_EQI(n->num_operands, 2);
  }
END_TEST

DEF_TEST(parse_multiple_instructions)
  parser p;
  parser_init(&p,
    "mov a, b\n"
    "\n"
    "end\n"
    "\n"
    "msg 'foo >', a, b, c, d\n"
  );

  {
    instruction_node* n = parser_parse_instruction(&p);
    ASSERT_NOT_NULL(n);
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);
  
    ASSERT_NOT_NULL(n->opcode);
    ASSERT_EQS(n->opcode->s, "mov");
  
    ASSERT_EQI(n->num_operands, 2);
    
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);
    
    ASSERT_EQI(n->operands[0]->type, NODE_TYPE_OPERAND_SIMPLE);
    ASSERT_EQS(((operand_simple_node *) n->operands[0])->token->s, "a");
    
    ASSERT_EQI(n->operands[1]->type, NODE_TYPE_OPERAND_SIMPLE);
    ASSERT_EQS(((operand_simple_node *) n->operands[1])->token->s, "b");
  }

  {
    instruction_node* n = parser_parse_instruction(&p);
    ASSERT_NOT_NULL(n);
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);
  
    ASSERT_NOT_NULL(n->opcode);
    ASSERT_EQS(n->opcode->s, "end");
  
    ASSERT_EQI(n->num_operands, 0);
  }

  {
    instruction_node* n = parser_parse_instruction(&p);
    ASSERT_NOT_NULL(n);
    ASSERT_EQI(n->type, NODE_TYPE_INSTRUCTION);
  
    ASSERT_NOT_NULL(n->opcode);
    ASSERT_EQS(n->opcode->s, "msg");
  
    ASSERT_EQI(n->num_operands, 5);
  
    ASSERT_EQI(((operand_simple_node *) n->operands[0])->token->type, TT_STRING);
    ASSERT_EQS(((operand_simple_node *) n->operands[0])->token->s, "foo >");
  
    ASSERT_EQI(((operand_simple_node *) n->operands[1])->token->type, TT_SYMBOL);
    ASSERT_EQS(((operand_simple_node *) n->operands[1])->token->s, "a");
  
    ASSERT_EQI(((operand_simple_node *) n->operands[2])->token->type, TT_SYMBOL);
    ASSERT_EQS(((operand_simple_node *) n->operands[2])->token->s, "b");
    
    ASSERT_EQI(((operand_simple_node *) n->operands[3])->token->type, TT_SYMBOL);
    ASSERT_EQS(((operand_simple_node *) n->operands[3])->token->s, "c");
    
    ASSERT_EQI(((operand_simple_node *) n->operands[4])->token->type, TT_SYMBOL);
    ASSERT_EQS(((operand_simple_node *) n->operands[4])->token->s, "d");
  }
END_TEST

DEF_TEST(parse_labeled_instructions)
  //  TODO: update operands
  // parser p;
  // parser_init(&p,
  //   "foo:\n"
  //   " mov a, 5\n"
  //   " mov b, a\n"
  //   " ret\n"
  // );

  // label_node* label_node = parser_parse_label(&p);
  // ASSERT_NOT_NULL(label_node);
  // ASSERT_NOT_NULL(label_node->name);
  // ASSERT_EQI(label_node->name->type, TT_SYMBOL);
  // ASSERT_EQS(label_node->name->s, "foo");
  // ASSERT_EQI(label_node->num_instructions, 3);

  // instruction_node** ins = label_node->instructions;
  // ASSERT_EQS(ins[0]->opcode->s, "mov");
  // ASSERT_EQS(ins[0]->operands[0]->s, "a");
  // ASSERT_EQI(ins[0]->operands[1]->i, 5);

  // ASSERT_EQS(ins[1]->opcode->s, "mov");
  // ASSERT_EQS(ins[1]->operands[0]->s, "b");
  // ASSERT_EQS(ins[1]->operands[1]->s, "a");

  // ASSERT_EQS(ins[2]->opcode->s, "ret");
  // ASSERT_NULL(ins[2]->operands);
END_TEST

DEF_TEST(is_next_tokens_a_label)
  parser p;
  parser_init(&p,
    "foo:\n"
    " mov a, b\n"
  );

  {
    ASSERT(is_next_tokens_a_label(&p));

    // Ensure push back
    token* t0 = parser_next_token(&p);
    token* t1 = parser_next_token(&p);

    ASSERT_EQI(t0->type, TT_SYMBOL);
    ASSERT_EQS(t0->s, "foo");

    ASSERT_EQI(t1->type, TT_COLON);
    ASSERT_EQC(t1->c, ':');
  }


  {
    parser_init(&p, "mov a, 5");

    ASSERT(!is_next_tokens_a_label(&p));

    token* t0 = parser_next_token(&p);
    token* t1 = parser_next_token(&p);

    ASSERT_EQI(t0->type, TT_SYMBOL);
    ASSERT_EQS(t0->s, "mov");

    ASSERT_EQI(t1->type, TT_SYMBOL);
    ASSERT_EQS(t1->s, "a");
  }
END_TEST

DEF_TEST(parser_parse)
  parser p;
  parser_init(&p,
    "; My first program\n"
    "mov  a, 5\n"
    "inc  a\n"
    "call function\n"
    "msg  '(5+1)/2 = ', a    ; output message\n"
    "end\n"
    "\n"
    "function:\n"
    "    div  a, 2\n"
    "    ret\n"
    "foo:\n"
    " mov a, b\n"
    " mov b, a\n"
    " ret\n"
    "\n"
  );

  top_level_node* n = parser_parse(&p);
  ASSERT_NOT_NULL(n);
  {
    ASSERT_EQI(n->num_instructions, 5);
    char* top_level_insns_opcodes[] = {"mov","inc","call","msg","end"};
    int top_level_insns_num_operands[] = {2, 1, 1, 2, 0};

    for (int i = 0; i < n->num_instructions; i++) {
      ASSERT_EQS(n->instructions[i]->opcode->s, top_level_insns_opcodes[i]);
      ASSERT_EQI(n->instructions[i]->num_operands, top_level_insns_num_operands[i]);
    }
  }

  {
    ASSERT_EQI(n->num_labels, 2);
    char* label_names[] = {"function", "foo"};
    char* label_insns_opcodes[][3] = {{"div", "ret"}, {"mov", "mov", "ret"}};

    for (int i = 0; i < n->num_labels; i++) {
      label_node* l = n->labels[i];

      ASSERT_EQS(l->name->s, label_names[i]);

      for (int j = 0; j < l->num_instructions; j++) {
        ASSERT_EQS(l->instructions[j]->opcode->s, label_insns_opcodes[i][j]);
      }
    }
  }
END_TEST

DEF_TEST(interp_test_opcode_conversion)
  parser p;
  parser_init(&p, "mov a, 5");
  instruction_node* n = parser_parse_instruction(&p);

  ASSERT_NOT_NULL(n);

  instruction insn;
  convert_instruction_node(n, &insn);

  ASSERT_EQI(insn.opcode, OPCODE_MOV);
  ASSERT_EQI(insn.num_operands, 2);

  ASSERT_EQI(insn.operands[0].type, OPERAND_REG);
  ASSERT_EQI(insn.operands[0].reg_index, 0); // :reg_index

  ASSERT_EQI(insn.operands[1].type, OPERAND_INT);
  ASSERT_EQI(insn.operands[1].int_value, 5);

  ASSERT_POS(insn.opcode_pos, 1, 0, 3);
  ASSERT_POS(insn.operands[0].pos, 1, 4, 5);
  ASSERT_POS(insn.operands[1].pos, 1, 7, 8);
END_TEST

static void program_init_and_build(program* prog, const char* code) {
  parser p;
  parser_init(&p, code);

  top_level_node* n = parser_parse(&p);

  prog->error_handler = p.error_handler;

  program_build(prog, n);
}

DEF_TEST(interp_test_branch_insns)
  char* tests[] = {
      "cmp 2, 1      \n"
      "jg is_bigger  \n"
      "msg 'fail'    \n"
      "end           \n"
      "is_bigger:    \n"
      "  msg 'ok'    \n"

      "cmp 1, 2      \n"
      "jl is_lesser  \n"
      "msg 'fail'    \n"
      "end           \n"
      "is_lesser:    \n"
      "  msg 'ok'    \n"

      "cmp 1, 1         \n"
      "jge is_ge_or_eq  \n"
      "msg 'fail'       \n"
      "end              \n"
      "is_ge_or_eq:     \n"
      "  msg 'ok'       \n"

      "cmp 1, 1         \n"
      "jle is_le_or_eq  \n"
      "msg 'fail'       \n"
      "end              \n"
      "is_le_or_eq:     \n"
      "  msg 'ok'       \n"

      "jmp ok           \n"
      "msg 'fail'       \n"
      "end              \n"
      "ok:              \n"
      "  msg 'ok'       \n"
  };
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    char* r = interp(tests[i]);
    ASSERT_EQS(r, "ok");
  }
END_TEST

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

DEF_TEST(interp_errors)
  const char* test_codes[] = {
    "mov 5, a",
    "jmp what",
    "inc 1",
    "mov invalidreg, 123",
    "cmp 'foo', b",
    "jne 1",
    "sub a, 'foobar'",
    "mov",
    "mov a",
    "inc a,b,c,d",
    "jne",
    "ret 123",
    "cmp",
  };
  const char* test_errors[] = {
    "opcode 'mov' requires a 'register' or a 'memory address' as its first operand, but got a 'integer'",
    "label not defined",
    "opcode 'inc' requires a 'register' as its first operand, but got a 'integer'",
    "invalid register",
    "opcode 'cmp' requires a 'register' or a 'integer' as its first operand, but got a 'string'",
    "opcode 'jne' requires a 'label' as its first operand, but got a 'integer'",
    "opcode 'sub' requires a 'register' or a 'integer' as its second operand, but got a 'string'",
    "incorrect number of operands for opcode 'mov'. Required: 2, got: 0",
    "incorrect number of operands for opcode 'mov'. Required: 2, got: 1",
    "incorrect number of operands for opcode 'inc'. Required: 1, got: 4",
    "incorrect number of operands for opcode 'jne'. Required: 1, got: 0",
    "incorrect number of operands for opcode 'ret'. Required: 0, got: 1",
    "incorrect number of operands for opcode 'cmp'. Required: 2, got: 0",
  };
  ASSERT_EQI(ARR_LEN(test_codes), ARR_LEN(test_errors));

  for (int i = 0; i < sizeof(test_codes) / sizeof(test_codes[0]); i++) {
    src_pos out_pos;
    char* out_msg;

    void** data = malloc(sizeof(void*) * 2);
    data[0] = &out_msg;
    data[1] = &out_pos;

    error_handler x = {
      .handler_fn = test_error_handler,
      .handler_data = data
    };

    program p;
    program_init_and_build(&p, test_codes[i]);
    p.error_handler = &x;
    program_check(&p);

    ASSERT_EQS(out_msg, (char*) test_errors[i]);
  }
END_TEST

DEF_TEST(interp_run)
  {
    program p;
    program_init_and_build(&p,
      "mov a, 5\n"
      "mov b, 5\n"
      "add a, b\n"
      "msg 'a is ', a, ' and b is ', b\n"
    );
    program_check(&p);
    char *msg = program_run(&p);
    ASSERT_EQS(msg, "a is 10 and b is 5");
  }

  {
    program p;
    program_init_and_build(&p,
      "mov   a, 81         ; value1\n"
      "mov   b, 153        ; value2\n"
      "call  init\n"
      "call  proc_gcd\n"
      "call  print\n"
      "end\n"
      "\n"
      "proc_gcd:\n"
      "    cmp   c, d\n"
      "    jne   loop\n"
      "    ret\n"
      "\n"
      "loop:\n"
      "    cmp   c, d\n"
      "    jg    a_bigger\n"
      "    jmp   b_bigger\n"
      "\n"
      "a_bigger:\n"
      "    sub   c, d\n"
      "    jmp   proc_gcd\n"
      "\n"
      "b_bigger:\n"
      "    sub   d, c\n"
      "    jmp   proc_gcd\n"
      "\n"
      "init:\n"
      "    cmp   a, 0\n"
      "    jl    a_abs\n"
      "    cmp   b, 0\n"
      "    jl    b_abs\n"
      "    mov   c, a            ; temp1\n"
      "    mov   d, b            ; temp2\n"
      "    ret\n"
      "\n"
      "a_abs:\n"
      "    mul   a, -1\n"
      "    jmp   init\n"
      "\n"
      "b_abs:\n"
      "    mul   b, -1\n"
      "    jmp   init\n"
      "\n"
      "print:\n"
      "    msg   'gcd(', a, ', ', b, ') = ', c\n"
      "    ret \n"
    );

    program_check(&p);
    char* msg = program_run(&p);
    ASSERT_EQS(msg, "gcd(81, 153) = 9");
  }
END_TEST

void interp_suite() {
  SUITE_INIT(interp)
    //REPORT_ONLY_FAILS();
    ADD_TEST(interp_test_opcode_conversion);
    ADD_TEST(interp_run);
    ADD_TEST(interp_test_branch_insns);
    ADD_TEST(interp_errors);
  SUITE_RUN
}

void lexer_suite(){
  SUITE_INIT(lexer)
    // REPORT_ONLY_FAILS();
    ADD_TEST(lexer_eof);
    ADD_TEST(lexer_peek_next);
    ADD_TEST(lexer_int_token);
    ADD_TEST(lexer_str_token);
    ADD_TEST(lexer_misc_token);
    ADD_TEST(lexer_multiple);
    ADD_TEST(lexer_positions);
    ADD_TEST(lexer_error_unclosed_string);
  SUITE_RUN
}

void parser_suite() {
  SUITE_INIT(parser)
    // REPORT_ONLY_FAILS();
    ADD_TEST(parse_multiple_instructions);
    ADD_TEST(parse_instruction_without_operands);
    ADD_TEST(parse_instruction_with_operands);
    ADD_TEST(parse_labeled_instructions);
    ADD_TEST(is_next_tokens_a_label);
    ADD_TEST(parser_parse);
    ADD_TEST(parser_errors);
  SUITE_RUN
}

void run_tests() {
  lexer_suite();
  parser_suite();
  interp_suite();
}

int main(void) {
  run_tests();

  alloc_spy_report();
}