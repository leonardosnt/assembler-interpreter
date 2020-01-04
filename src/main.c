#include "pch.h"

#include "alloc_spy.h"

#include "lexer.h"
#include "parser.h"
#include "interp.h"
#include "genc.c"

void disasm(program* prg);
void read_file_fully(FILE* fp, char** data, long* data_len);
void run_from_file(const char* path);

int main(int argc, const char** argv) {
  if (argc < 2) {
    printf("Use interp [file]\n");
    return 1;
  }

  run_from_file(argv[1]);
  // alloc_spy_report();
  return 0;
}

void read_file_fully(FILE* fp, char** data, long* data_len) {
  // First we get the size of the file
  fseek(fp, 0, SEEK_END);
  long flen = ftell(fp);
  rewind(fp);

  // Alloc what we need to read the entire file
  char *fbuf = (char*) malloc(flen + 1);
  if (!fbuf) {
    printf("failed to alloc memory to read the file\n");
    exit(1);
  }

  // Read the contents to the buffer
  fread(fbuf, flen, 1, fp);
  fclose(fp);
  fbuf[flen] = '\0';

  *data = fbuf;
  *data_len = flen;
}

void run_from_file(const char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    printf("failed to open file %s\n", path);
    exit(2);
  }

  long data_len = 0;
  char* data = NULL;
  read_file_fully(fp, &data, &data_len);

  if (0) {
    PERF_START(gen_c);

    parser p;

    parser_init(&p, data);

    top_level_node* n = parser_parse(&p);

    program prog;
    prog.error_handler = p.error_handler;

    program_build(&prog, n);
    program_check(&prog);

    // XXX TEMP REPLACE .asm with .c
    char* gen_c_out_path = strdup(path);
    size_t len = strlen(gen_c_out_path);
    gen_c_out_path[len - 3] = 'c';
    gen_c_out_path[len - 2] = '\0';

    FILE* gen_outfp = fopen(gen_c_out_path, "w");
    gen_c(&prog, gen_outfp);
    fclose(gen_outfp);
    PERF_STOP(gen_c);
  }

  char* result = interp(data);
  printf("Result: '%s'\n", result);
}

void disasm_single_insn(program* prg, int insn_index, instruction in, char **ident) {
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
    } else {
      printf("<unhandled op type %d>", op.type);
    }
    if (j != in.num_operands - 1) printf(",");
  }
}

void disasm(program* prg) {
  // Disasm
  char* ident = "";
  for (int i = 0; i < prg->num_instructions; i++) {
    disasm_single_insn(prg, i, prg->instructions[i], &ident);

    printf("\n");
  }
}