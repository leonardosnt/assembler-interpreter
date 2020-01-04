#ifndef __INTERP_H__
#define __INTERP_H__

#include <stdint.h>

#include "pch.h"
#include "lexer.h"
#include "parser.h"

const int INVALID_REGISTER_INDEX = -1;

typedef enum opcode {
  OPCODE_INVALID,

  OPCODE_MOV,
  OPCODE_ADD,
  OPCODE_SUB,
  OPCODE_MUL,
  OPCODE_DIV,
  OPCODE_INC,
  OPCODE_DEC,
  OPCODE_JMP,
  OPCODE_JNE,
  OPCODE_JE,
  OPCODE_JGE,
  OPCODE_JG,
  OPCODE_JLE,
  OPCODE_JL,
  OPCODE_CALL,
  OPCODE_RET,
  OPCODE_CMP,
  OPCODE_MSG,
  OPCODE_END,

  // :custom
  OPCODE_PRINT,
  OPCODE_PUSH,
  OPCODE_POP,

  // TODO: we could use CALL to call an extern routine...?
  // (malloc, free)
  OPCODE_MALLOC,
  OPCODE_MFREE
} opcode;

static const char* opcode_names[] = {
  "<invalid>",
  "mov",
  "add",
  "sub",
  "mul",
  "div",
  "inc",
  "dec",
  "jmp",
  "jne",
  "je",
  "jge",
  "jg",
  "jle",
  "jl",
  "call",
  "ret",
  "cmp",
  "msg",
  "end",
  "print",
  "push",
  "pop",
  "malloc",
  "mfree"
};

typedef enum operand_type {
  OPERAND_REG,
  OPERAND_INT,
  OPERAND_BRANCH,
  OPERAND_UNRESOLVED_BRANCH,
  OPERAND_STR,
  OPERAND_MEM_ADDRESS,
} operand_type;

typedef struct operand {
  operand_type type;
  union {
    int reg_index;
    int64_t int_value;
    int branch_index;
    // if type is UNRESOLVED_BRANCH this will hold
    // the label name until it's resolved...
    char* str;
  };
  // XXX
  // For now we are using this to store the mem_address offset.
  // Later we may want to have a separated struct for this
  // (and for unresolved branch too, maybe.)
  // :operand_struct
  int64_t extra;
  src_pos pos;
} operand;

 static const char* friendly_operand_type_names[] = {
  "register",
  "integer",
  "label",
  "unknown label",
  "string",
  "memory address"
};

typedef struct instruction {
  opcode opcode;

  int num_operands;
  operand* operands; // array of operands. @NOTE: should it be pointers? not sure

  // TODO: use ptr? then point it to the token's pos
  // (we first need to use src_pos in the token)
  src_pos opcode_pos;
} instruction;

typedef struct resolved_label {
  char* name;
  int instruction_index;
  // Used only for reporting duplicated labels...
  // Not sure if it be stored here. But we need it to be able to report
  // duplicated labels in program_check...
  src_pos name_pos;
} resolved_label;

typedef struct program { // TODO: not sure how to call this
  instruction* instructions;
  int num_instructions;

  int num_resolved_labels;
  resolved_label* resolved_labels;

  error_handler* error_handler;
  // TOOD: add flags? FLAG_DEBUGGING, FLAG_PRINT_MSG
} program;

static int count_instructions(top_level_node* n) {
  int total = n->num_instructions;
  for (int i = 0; i < n->num_labels; i++) {
    total += n->labels[i]->num_instructions;
  }
  return total;
}

static opcode get_opcode(char* name) {
  // TODO: better lookup... ?
  for (int i = 1; i < sizeof(opcode_names) / sizeof(opcode_names[0]); i++) {
    if (strcmp(opcode_names[i], name) == 0) {
      return (opcode) i;
    }
  }
  return OPCODE_INVALID;
}

static void token_copy_position(src_pos* to, token* from) {
  to->line_number = from->pos.line_number;
  to->col_start = from->pos.col_start;
  to->col_end = from->pos.col_end;
}

static int token_to_reg_index(token* tok) {
  char c = tok->s[0];
  int sym_len = tok->pos.col_end - tok->pos.col_start;
  if (c < 'a' || c > 'z' || sym_len > 1) {
    // Mark as invalid so the program_check can report it
    return INVALID_REGISTER_INDEX;
  }
  return (int) c - 'a';
}

void convert_operand(opcode opc, node* node, operand* op) {
  if (node->type == NODE_TYPE_OPERAND_SIMPLE) {
    token* tok = ((operand_simple_node* ) node)->token;
    switch (tok->type) {
      case TT_INT:
        op->type = OPERAND_INT;
        op->int_value = tok->i;
        break;
      case TT_STRING:
        op->type = OPERAND_STR;
        op->str = tok->s;
        break;
      case TT_SYMBOL:
        // If it's a symbol it is either a register or a label
        if (opc >= OPCODE_JMP && opc <= OPCODE_CALL) {
          op->type = OPERAND_UNRESOLVED_BRANCH;
          op->str = tok->s;
        } else {
          op->type = OPERAND_REG;
          op->reg_index = token_to_reg_index(tok);
        }
        break;
      default:
        assert(0 && "should not reach here");
        break;
    }
    token_copy_position(&op->pos, tok);
  }
  else if (node->type == NODE_TYPE_OPERAND_MEM_ADDRESS) {
    operand_mem_address_node* n = (operand_mem_address_node*) node;
    op->type = OPERAND_MEM_ADDRESS;
    op->reg_index = token_to_reg_index(n->register_token);
    op->extra = n->offset_token == NULL ? 0 : n->offset_token->i;
    op->pos.line_number = n->pos.line_number;
    op->pos.col_start = n->pos.col_start;
    op->pos.col_end = n->pos.col_end;
  }
  else assert(0);
}

// TOOD: better naming? prefix
void convert_instruction_node(instruction_node* n, instruction* ins) {
  opcode opc = get_opcode(n->opcode->s);

  ins->opcode = opc;
  ins->operands = NULL;
  ins->num_operands = n->num_operands;
  token_copy_position(&ins->opcode_pos, n->opcode);

  if (n->num_operands > 0) {
    ins->operands = (operand*) malloc(n->num_operands * sizeof(operand));
    for (int i = 0; i < n->num_operands; i++) {
      convert_operand(opc, n->operands[i], &ins->operands[i]);
    }
  }
}

void program_build(program* p, top_level_node* top_node) {
  int total_instructions = count_instructions(top_node);

  instruction* instructions = (instruction*) malloc(total_instructions * sizeof(instruction));
  int cur_instruction = 0;

  for (int i = 0; i < top_node->num_instructions; i++) {
    convert_instruction_node(top_node->instructions[i],
                             &instructions[cur_instruction]);
    cur_instruction++;
  }

  // @perf
  // Simple implementation...
  // Uses a list to map between 'label name' and 'instruction index'.
  resolved_label* resolved_labels = (resolved_label*) malloc(
    top_node->num_labels * sizeof(resolved_label));
  int cur_resolved_label = 0;

  for (int i = 0; i < top_node->num_labels; i++) {
    label_node* label = top_node->labels[i];

    // 'Register' resolved label
    resolved_labels[cur_resolved_label].name = label->name->s;
    resolved_labels[cur_resolved_label].instruction_index = cur_instruction;
    token_copy_position(&resolved_labels[cur_resolved_label].name_pos, label->name);
    cur_resolved_label++;

    for (int j = 0; j < label->num_instructions; j++) {
      convert_instruction_node(label->instructions[j], &instructions[cur_instruction]);
      cur_instruction++;
    }
  }

  assert(cur_instruction == total_instructions);

  // Resolve labels
  // @PERF
  // O(n^2) lookup
  for (int i = 0; i < cur_instruction; i++) {
    instruction in = instructions[i];

    if (in.num_operands == 0 || in.operands[0].type != OPERAND_UNRESOLVED_BRANCH) {
      continue;
    }

    char* target_label = in.operands[0].str;
    for (int i = 0; i < top_node->num_labels; i++) {
      if (strcmp(resolved_labels[i].name, target_label) == 0) {
        in.operands[0].str = NULL;

        in.operands[0].type = OPERAND_BRANCH;
        in.operands[0].branch_index = resolved_labels[i].instruction_index;
        break;
      }
    }
  }

  p->num_resolved_labels = cur_resolved_label;
  p->resolved_labels = resolved_labels;
  p->instructions = instructions;
  p->num_instructions = cur_instruction;
}

char* program_get_label_by_index(program* p, int insn_index) {
  for (int i = 0; i < p->num_resolved_labels; i++) {
    if (p->resolved_labels[i].instruction_index == insn_index) {
      return p->resolved_labels[i].name;
    }
  }
  return NULL;
}

static void program_report_error(program* p, char* msg, src_pos* pos) {
  assert(pos);
  assert(p->error_handler);
  assert(p->error_handler->handler_fn);
  p->error_handler->handler_fn(p->error_handler, msg,pos);
}

static void program_report_errorf(program* p, src_pos* pos, const char* fmt, ...) {
  static char msg_buf[1024];

  va_list args;
  va_start(args, fmt);
  vsnprintf(msg_buf, 1024, fmt, args);
  va_end(args);

  program_report_error(p, msg_buf, pos);
}

static void require_operand_types(
  program* p, instruction* insn, int op_index,
  operand_type types[], int num_types
) {
  operand operand = insn->operands[op_index];
  for (int i = 0; i < num_types; i++) {
    if (operand.type == types[i]) return;
  }

  assert(op_index <= 2);
  const char* op_pos_desc[] = {"first", "second"};

  #define MSG_BUF_LEN 300
  static char msg_buf[MSG_BUF_LEN];
  static char* msg_buf_ptr = msg_buf;

  msg_buf_ptr += snprintf(msg_buf_ptr, MSG_BUF_LEN, "opcode '%s' requires a ",
    opcode_names[insn->opcode]);

  for (int i = 0; i < num_types; i++) {
    msg_buf_ptr += sprintf(msg_buf_ptr, "'%s'", friendly_operand_type_names[types[i]]);
    if (i < num_types - 1) {
      static const char* separators[] = {", ", " or "};
      const char* separator = separators[i + 2 > num_types ? 0 : 1];
      msg_buf_ptr += sprintf(msg_buf_ptr, "%s", separator);
    }
  }

  snprintf(msg_buf_ptr, MSG_BUF_LEN, " as its %s operand, but got a '%s'",
    op_pos_desc[op_index],
    friendly_operand_type_names[operand.type]);

  program_report_error(p, msg_buf, &operand.pos);
  #undef MSG_BUF_LEN
}

static void check_operands(program* p, instruction* insn) {
  const char** operand_type_desc = friendly_operand_type_names;

  opcode opcode = insn->opcode;
  int num_operands = insn->num_operands;
  operand* operands = insn->operands;

  // Check for invalid registers
  for (int i = 0; i < insn->num_operands; i++) {
    operand op = insn->operands[i];
    if (op.type == OPERAND_REG && op.reg_index == INVALID_REGISTER_INDEX) {
      program_report_error(p, "invalid register", &op.pos);
      return;
    }
    if (op.type == OPERAND_MEM_ADDRESS && op.reg_index == INVALID_REGISTER_INDEX) {
      // TODO: report only the position of the register symbol?
      program_report_error(p, "invalid register specified in memory address", &op.pos);
      return;
    }
  }

  #define REQUIRE_NUM_OPERANDS(req_num_operands)         \
    if (num_operands != req_num_operands) {              \
      program_report_errorf(p, &insn->opcode_pos,        \
        "incorrect number of operands for opcode '%s'. " \
        "Required: "#req_num_operands", got: %d",        \
        opcode_names[insn->opcode], num_operands);       \
      return;                                            \
    }

  #define REQUIRE_OPERAND_TYPE(op_index, op_type)                          \
    if (operands[op_index].type != op_type) {                              \
      program_report_errorf(p, &operands[op_index].pos,                    \
        "opcode '%s' requires a '%s' as its %s operand, but got a '%s'",   \
        opcode_names[opcode], operand_type_desc[op_type],                  \
        (op_index == 0 ? "first" : "second"),                              \
        operand_type_desc[operands[op_index].type]);                       \
    }

  #define REQUIRE_OPERAND_TYPES(op_index, ...) do {           \
    operand_type types[] = { __VA_ARGS__ };                \
    require_operand_types(p, insn, op_index, types, sizeof(types) / sizeof(types[0])); \
  } while (0);

  switch (opcode) {
    // 'reg, reg'   or   'reg, int'
    case OPCODE_MOV: case OPCODE_ADD: case OPCODE_SUB:
    case OPCODE_MUL: case OPCODE_DIV: {
      REQUIRE_NUM_OPERANDS(2);
      REQUIRE_OPERAND_TYPES(1, OPERAND_REG, OPERAND_INT, OPERAND_MEM_ADDRESS);
      break;
    }

    case OPCODE_INC: case OPCODE_DEC: case OPCODE_PUSH: case OPCODE_POP:
    case OPCODE_MFREE: {
      REQUIRE_NUM_OPERANDS(1);
      REQUIRE_OPERAND_TYPE(0, OPERAND_REG);
      break;
    }

    case OPCODE_JMP: case OPCODE_JNE: case OPCODE_JE: case OPCODE_JGE:
    case OPCODE_JG:  case OPCODE_JLE: case OPCODE_JL: case OPCODE_CALL: {
      REQUIRE_NUM_OPERANDS(1);

      if (operands[0].type == OPERAND_UNRESOLVED_BRANCH) {
        // TODO: label '$label_name' not defined.
        // We would need to have a reference to the name...
        // We can do this in two ways:
        //  - store the name in the operand struct (or store the operand_node itself)
        //  - get the range in the source code since we already have op.pos
        // :operand_struct
        program_report_error(p, "label not defined", &operands[0].pos);
        return;
      }

      REQUIRE_OPERAND_TYPE(0, OPERAND_BRANCH);
      break;
    }

    case OPCODE_RET: case OPCODE_END: {
      REQUIRE_NUM_OPERANDS(0);
      break;
    }

    case OPCODE_CMP: {
      REQUIRE_NUM_OPERANDS(2);
      REQUIRE_OPERAND_TYPES(0, OPERAND_REG, OPERAND_INT);
      REQUIRE_OPERAND_TYPES(1, OPERAND_REG, OPERAND_INT);
      break;
    }

    case OPCODE_MALLOC: {
      REQUIRE_NUM_OPERANDS(2);
      REQUIRE_OPERAND_TYPE(0, OPERAND_REG);
      REQUIRE_OPERAND_TYPE(1, OPERAND_REG);
      break;
    }

    // Nothing to check I guess
    case OPCODE_MSG:
    case OPCODE_PRINT: break;
    default: assert(0);
  }
}

static void check_instructions(program* p) {
  for (int i = 0; i < p->num_instructions; i++) {
    instruction insn = p->instructions[i];

    if (insn.opcode == OPCODE_INVALID) {
      program_report_error(p, "invalid opcode", &insn.opcode_pos);
      return;
    }

    check_operands(p, &insn);
  }
}

static void check_for_duplicated_labels(program *p) {
  // TODO @perf
  for (int i = 0; i < p->num_resolved_labels; i++) {
    resolved_label* l = &p->resolved_labels[i];

    for (int j = i + 1; j < p->num_resolved_labels; j++) {
      resolved_label* l2 = &p->resolved_labels[j];

      if (strcmp(l->name, l2->name) == 0) {
        program_report_errorf(p, &l2->name_pos, "duplicated label '%s'.", l2->name);
        return;
      }
    }
  }
}

void xxdisasm_single_insn(program* prg, int insn_index, instruction in, char **ident) {
  // @perf
  int i = insn_index;
  char* label_name = program_get_label_by_index(prg, i);
  if (label_name) {
    printf("%03d| %s:\n", i, label_name);
    *ident = "  ";
  }

  printf("%03d| %s%s", i, *ident, opcode_names[in.opcode]);

  for (int j = 0; j < in.num_operands; j++) {
    operand op = in.operands[j];
    if (op.type == OPERAND_REG) {
      printf(" %c", (char) ('a' + op.reg_index));
    } else if (op.type == OPERAND_INT)   {
      printf(" %I64d", op.int_value);
    } else if (op.type == OPERAND_STR) {
      printf(" '%s'", op.str);
    } else if (op.type == OPERAND_BRANCH) {
      char* label_name = program_get_label_by_index(prg, op.branch_index);
      assert(label_name);

      printf(" %d  (%s)", op.branch_index, label_name);
    } else if (op.type == OPERAND_UNRESOLVED_BRANCH) {
      printf(" <unknown label %s>", op.str);
    } else if (op.type == OPERAND_MEM_ADDRESS) {
      printf(" %I64i[%c]", op.extra, (char) ('a' + op.reg_index));
    } else {
      printf(" <unhandled op type %d>", op.type);
    }
    if (j != in.num_operands - 1) printf(",");
  }
}

void program_check(program* p) {
  check_for_duplicated_labels(p);
  check_instructions(p);
}

// TODO: convert opcodes to 'operand-specific' opcodes
// e.g:
//  mov reg, mem    -> OPCODE_MOV_RM
//  mov reg, reg    -> OPCODE_MOV_RR
//  mov mem, reg    -> OPCODE_MOV_MR
// this will eliminate the if's to check operand types...

#define MAX_CALL_STACK 1000
#define NUM_REGISTERS 26
#define MAX_MSG 1000
#define MAX_STACK 500

char* program_run(program* p) {
  char* msg = (char*) malloc(MAX_MSG);
  msg[0] = '\0';

  int64_t registers[NUM_REGISTERS] = {0};

  uint32_t pc = 0;
  int64_t cmp = 0;
  uint32_t call_stack_top = 0;
  uint32_t call_stack[MAX_CALL_STACK] = {0};

  int64_t stack[MAX_STACK] = {0}; // TODO: merge call_stack and stack
  uint32_t stack_top = 0;

  #define REG(op) (registers[op.reg_index])
  #define REG_OR_INT_OR_MEM(op) (op.type == OPERAND_REG  \
    ? REG(op) : op.type == OPERAND_INT \
    ? op.int_value : *((int64_t*) (REG(op) + op.extra)))

  while (pc < p->num_instructions) {
    instruction in = p->instructions[pc];
    operand* ops = in.operands;

    #define OP0 ops[0]
    #define OP1 ops[1]

    switch (in.opcode) {
      #define MEMORY_OR_REG_OP(op, rhs) {                    \
        int64_t* target = OP0.type == OPERAND_REG           \
            ? (int64_t*) &REG(OP0) /* register */           \
            : (int64_t*) (REG(OP0) + OP0.extra); /* memory */ \
        *target op rhs;                                      \
        break;                                               \
      }
      case OPCODE_MOV: MEMORY_OR_REG_OP(=, REG_OR_INT_OR_MEM(OP1));
      case OPCODE_INC: MEMORY_OR_REG_OP(+=, 1);
      case OPCODE_DEC: MEMORY_OR_REG_OP(-=, 1);
      case OPCODE_ADD: MEMORY_OR_REG_OP(+=, REG_OR_INT_OR_MEM(OP1));
      case OPCODE_SUB: MEMORY_OR_REG_OP(-=, REG_OR_INT_OR_MEM(OP1));
      case OPCODE_MUL: MEMORY_OR_REG_OP(*=, REG_OR_INT_OR_MEM(OP1));
      case OPCODE_DIV: {
        int d = REG_OR_INT_OR_MEM(OP1);
        if (d == 0) {
          program_report_errorf(p, &in.opcode_pos,
            "division by zero occurred while executing this instruction");
          goto end;
        }
        MEMORY_OR_REG_OP(/=, d);
      }

      #define BRANCH_IF(cond) {   \
        if (cond) {               \
          pc = OP0.branch_index;  \
          continue;               \
        }                         \
        break;                    \
      }
      case OPCODE_JMP: BRANCH_IF(true);
      case OPCODE_JNE: BRANCH_IF(cmp != 0);
      case OPCODE_JE:  BRANCH_IF(cmp == 0);
      case OPCODE_JGE: BRANCH_IF(cmp >= 0);
      case OPCODE_JG:  BRANCH_IF(cmp > 0);
      case OPCODE_JLE: BRANCH_IF(cmp <= 0);
      case OPCODE_JL:  BRANCH_IF(cmp < 0);

      case OPCODE_CALL: {
        if (call_stack_top >= MAX_CALL_STACK) {
          program_report_errorf(p, &in.opcode_pos, "callstack overflow");
          goto end;
        }
        call_stack[call_stack_top++] = pc + 1;
        pc = OP0.branch_index;
        continue;
      }
      case OPCODE_RET: {
        if (call_stack_top <= 0) {
          program_report_errorf(p, &in.opcode_pos, "callstack underflow");
          goto end;
        }
        pc = call_stack[--call_stack_top];
        continue;
      }
      case OPCODE_CMP: {
        cmp = REG_OR_INT_OR_MEM(OP0) - REG_OR_INT_OR_MEM(OP1);
        break;
      }
      case OPCODE_PUSH: {
        if (stack_top >= MAX_STACK) {
          program_report_errorf(p, &in.opcode_pos, "stack overflow");
          goto end;
        }
        stack[stack_top++] = REG(OP0);
        break;
      }
      case OPCODE_POP: {
        if (stack_top <= 0) {
          program_report_errorf(p, &in.opcode_pos, "stack underflow");
          goto end;
        }
        REG(OP0) = stack[--stack_top];
        break;
      }
      case OPCODE_MSG: {
        // XXX FIXME
        // TODO: MAX_MSG
        // this will write past MAX_MSG...
        // even when using snprintf
        char* msg_ptr = msg;
        for (int i = 0; i < in.num_operands; i++) {
          operand op = in.operands[i];
          switch (op.type) {
            case OPERAND_STR:
              msg_ptr += sprintf(msg_ptr, "%s", op.str);
              break;
            case OPERAND_INT:
              msg_ptr += sprintf(msg_ptr, "%I64d", op.int_value);
              break;
            case OPERAND_REG:
              msg_ptr += sprintf(msg_ptr, "%I64d", registers[op.reg_index]);
              break;
            default:
              sprintf(msg_ptr, "<unhandled operand type %d>", op.type);
              break;
          }
        }
        break;
      }

      case OPCODE_PRINT: {
        for (int i = 0; i < in.num_operands; i++) {
          operand op = in.operands[i];
          switch (op.type) {
            case OPERAND_STR:
              // XXX: workaround for printing new lines... @cleanup
              if (strlen(op.str) == 2 && op.str[0] == '\\' && op.str[1] == 'n') {
                printf("\n");
                break;
              }
              printf("%s", op.str);
              break;
            case OPERAND_INT:
              printf("%I64d", op.int_value);
              break;
            case OPERAND_REG:
              printf("%I64d", registers[op.reg_index]);
              break;
            default:
              printf("<unhandled operand type %d>", op.type);
              break;
          }
        }
        break;
      }

      case OPCODE_MALLOC: {
        // OP0 = register that holds the size
        // OP1 = output register to the address
        registers[OP1.reg_index] = (uint64_t) malloc(registers[OP0.reg_index]);
        break;
      }

      case OPCODE_MFREE: {
        free((void*) registers[OP0.reg_index]);
        break;
      }

      case OPCODE_END:
        goto end;

      default:
        assert(0 && "should not reach here");
        break;
    }

    pc++;
  }

  end:
  return msg;
}

char* interp(char* code) {
  PERF_START(interp);
  parser p;

  // PERF_START(parse_tokens);
  parser_init(&p, code);
  // PERF_STOP(parse_tokens);

  // PERF_START(parse_top_level);
  top_level_node* n = parser_parse(&p);
  // PERF_STOP(parse_top_level);

  program prog;
  prog.error_handler = p.error_handler;

  // PERF_START(program_build);
  program_build(&prog, n);
  // PERF_STOP(program_build);

  // PERF_START(program_check);
  program_check(&prog);
  // PERF_STOP(program_check);

  // PERF_START(program_run);
  char* res = program_run(&prog);
  // PERF_STOP(program_run);

  PERF_STOP(interp);
  return res;
}
#endif // __INTERP_H__
