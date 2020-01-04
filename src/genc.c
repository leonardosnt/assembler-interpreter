#include "interp.h"

typedef struct gen_state {
  program* prog;
  int ret_label_counter;
  FILE* out;
} gen_state;

void gen_instruction(gen_state* gen, instruction* insn) {
  FILE* out = gen->out;
  operand* ops = insn->operands;

  #define EMIT_LINE fprintf(out, "%s", "\n")
  #define EMIT(str) fprintf(out, "%s", str)
  #define EMIT_INDENTED(str) fprintf(out, "  %s", str)
  #define EMITF(fmt, ...) fprintf(out, fmt, __VA_ARGS__)
  #define EMITF_INDENTED(fmt, ...) fprintf(out, "  "fmt, __VA_ARGS__)

  #define EMIT_REG(op) EMITF("r%c", 'a' + op.reg_index)

  #define EMIT_REG_OR_INT(op) (op.type == OPERAND_REG  \
    ? EMIT_REG(op) \
    : EMITF("%I64d", op.int_value))
  #define EMIT_REG_IDENT(op) EMIT("  "); EMIT_REG(op);

  #define OP0 ops[0]
  #define OP1 ops[1]
  #define LABEL(op) program_get_label_by_index(gen->prog, op.branch_index)

  EMITF("  /* %5s */", opcode_names[insn->opcode]);

  switch (insn->opcode) {
    case OPCODE_MOV:
      EMIT_REG_IDENT(OP0);
      EMIT(" = ");
      EMIT_REG_OR_INT(OP1);
      EMIT(";");
      EMIT_LINE;
      break;

    case OPCODE_INC: EMIT_REG_IDENT(OP0); EMIT("++;"); EMIT_LINE; break;
    case OPCODE_DEC: EMIT_REG_IDENT(OP0); EMIT("--;"); EMIT_LINE; break;

    #define EMIT_MATH_OP(op) \
      EMIT_REG_IDENT(OP0);  \
      EMIT(" "op" "); \
      EMIT_REG_OR_INT(OP1); \
      EMIT(";"); \
      EMIT_LINE;
    case OPCODE_ADD: EMIT_MATH_OP("+="); break;
    case OPCODE_SUB: EMIT_MATH_OP("-="); break;
    case OPCODE_MUL: EMIT_MATH_OP("*="); break;
    case OPCODE_DIV: EMIT_MATH_OP("/="); break; // TODO: check divbyzero

    case OPCODE_JMP: EMITF_INDENTED("goto %s;", LABEL(OP0)); EMIT_LINE; break;

    #define EMIT_BRANCH_IF(cond) \
      EMITF_INDENTED("if (%s) goto %s;", #cond, LABEL(OP0)); EMIT_LINE; break;
    case OPCODE_JNE: EMIT_BRANCH_IF(cmp != 0);
    case OPCODE_JE:  EMIT_BRANCH_IF(cmp == 0);
    case OPCODE_JGE: EMIT_BRANCH_IF(cmp >= 0);
    case OPCODE_JG:  EMIT_BRANCH_IF(cmp > 0);
    case OPCODE_JLE: EMIT_BRANCH_IF(cmp <= 0);
    case OPCODE_JL:  EMIT_BRANCH_IF(cmp < 0);

    // TODO: add explanation
    case OPCODE_CALL:
      EMITF_INDENTED("cs[csp++] = &&__ret_%d; goto %s; __ret_%d:",
          gen->ret_label_counter,
          LABEL(OP0),
          gen->ret_label_counter);
      EMIT_LINE;
      gen->ret_label_counter++;
      break;

    case OPCODE_RET:
      EMIT_INDENTED("goto *cs[--csp];");
      EMIT_LINE;
      break;

    case OPCODE_CMP:
      EMIT_INDENTED("cmp = ");
      EMIT_REG_OR_INT(OP0);
      EMIT(" - ");
      EMIT_REG_OR_INT(OP1);
      EMIT(";");
      EMIT_LINE;
      break;

    case OPCODE_PRINT:
    case OPCODE_MSG: {
      if (insn->opcode == OPCODE_MSG)
        EMIT_INDENTED("snprintf(msg, 1000, ");
      else
        EMIT_INDENTED("printf(");

      // Emit format
      EMIT("\"");
      for (int i = 0; i < insn->num_operands; i++) {
          operand op = insn->operands[i];
          switch (op.type) {
            case OPERAND_STR: EMIT("%s"); break;
            case OPERAND_INT:
            case OPERAND_REG: EMIT("%d"); break;
            default: assert(0); break;
          }
      }
      EMIT("\", ");
      // Emit values
      for (int i = 0; i < insn->num_operands; i++) {
          operand op = insn->operands[i];
          switch (op.type) {
            case OPERAND_STR: EMITF("\"%s\"", op.str); break;
            case OPERAND_INT:
            case OPERAND_REG: EMIT_REG_OR_INT(op); break;
            default: assert(0); break;
          }
          if (i < insn->num_operands - 1) {
            EMIT(", ");
          }
      }
      EMIT(");");
      EMIT_LINE;
      break;
    }

    case OPCODE_PUSH: {
      EMIT_INDENTED("stack[sp++] = ");
      EMIT_REG(OP0);
      EMIT(";");
      EMIT_LINE;
      break;
    }

    case OPCODE_POP: {
      EMIT_REG(OP0);
      EMIT_INDENTED("= stack[--sp];");
      EMIT_LINE;
      break;
    }

    case OPCODE_END:
      EMIT_INDENTED("goto __end;");
      EMIT_LINE;
      break;

    default:
      printf("opcode not implemented: %s\n", opcode_names[insn->opcode]);
      break;
  }
}

void gen_insns(gen_state* gen, instruction* instructions, int num_instructions) {
  for (int i = 0; i < num_instructions; i++) {
    char* label;
    if ((label = program_get_label_by_index(gen->prog, i)) != NULL) {
      fprintf(gen->out, "%s:\n", label);
    }
    gen_instruction(gen, &instructions[i]);
  }
}

// TODO: gen used registers...
// ra, rb, rc, rd
// forward decl them

const char* start = "\
#include <stdio.h>            \n\
#include <stdint.h>           \n\
int main(void) {              \n\
  char msg[1000] = {0};       \n\
  void* cs[1000] = {0};       \n\
  uint16_t csp = 0;           \n\
  int64_t cmp = 0;            \n\
  uint64_t stack[1000] = {0}; \n\
  uint16_t sp = 0;            \n\
";

const char* end = "\
  __end:    \n\
  printf(\"%s\\n\", msg); \n\
  return 0; \n\
} \n\
";

void gen_declaration_for_used_registers(gen_state* gen) {
  program* p = gen->prog;

  // use each bit (0-25) to mark that a register
  // was seen in the operands...
  int registers_used = 0;

  // Mark used registers
  for (int i = 0; i < p->num_instructions; i++) {
    instruction in = p->instructions[i];
    for (int j = 0; j < in.num_operands; j++) {
      operand op = in.operands[j];
      if (op.type == OPERAND_REG) {
        registers_used |= 1 << op.reg_index;
      }
    }
  }

  if (!registers_used) return;

  // Gen declaration (and initialize with zero).
  // Like: int ra=0, rb=0, rc=0;
  fprintf(gen->out, "  int64_t ");
  bool is_first = true;
  for (int i = 0; i < 26; i++) {
    if ((registers_used & (1 << i)) != 0) {
      if (!is_first) {
        fprintf(gen->out, ", ");
      }
      is_first = false;
      fprintf(gen->out, "r%c=0", 'a' + i);
    }
  }
  fprintf(gen->out, ";");
}

void gen_c(program* p, FILE* out) {
  gen_state gen_state = {
    .prog = p,
    .ret_label_counter = 0,
    .out = out
  };

  fprintf(out, "%s", start);
  gen_declaration_for_used_registers(&gen_state);
  fprintf(out, "\n  // Instructions\n");

  gen_insns(&gen_state, p->instructions, p->num_instructions);
  fprintf(out, "%s", end);
}