#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct src_pos {
  int line_number;
  int col_start;
  int col_end;
} src_pos;

typedef struct error_handler {
  void (*handler_fn)(struct error_handler* err_handler, char* msg, src_pos*);

  void** handler_data;
  const char* src_code;
} error_handler;

static bool get_line_range(const char* str, int line, int* line_start, int* line_end) {
  int cur_line = 1;
  int lstart = 0;
  int lend = 0;

  while (*str != '\0') {
    ++lend;
    if (*str == '\n') {
      if (line == cur_line) {
        *line_start = lstart;
        *line_end = lend - 1;
        return true;
      }
      cur_line++;
      lstart = lend;
    }
    str++;
  }
  // TODO: 'workaround for single line'
  // @cleanup
  if (line == cur_line) {
    *line_start = lstart;
    *line_end = lend;
    return true;
  }
  return false;
}

#ifdef INC_WINDOWS
  #define ERROR_COLOR FOREGROUND_RED | FOREGROUND_INTENSITY
  #define POSITION_COLOR FOREGROUND_GREEN | FOREGROUND_INTENSITY

  // Console color stuff
  #define START_CONSOLE_COLOR  \
      HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE); \
      CONSOLE_SCREEN_BUFFER_INFO console_info;  \
      WORD saved_attributes;  \
      GetConsoleScreenBufferInfo(h_console, &console_info); \
      saved_attributes = console_info.wAttributes;

  #define RESTORE_CONSOLE_COLOR SetConsoleTextAttribute(h_console, saved_attributes);

  #define SET_CONSOLE_COLOR(color) SetConsoleTextAttribute(h_console, color);
#else
  #define ERROR_COLOR 0
  #define POSITION_COLOR 0
  #define START_CONSOLE_COLOR
  #define RESTORE_CONSOLE_COLOR
  #define SET_CONSOLE_COLOR (void)
#endif

static void print_location(const char* src_code, src_pos* pos) {
  START_CONSOLE_COLOR;

  int line_start, line_end;
  if (
    pos->line_number == 0 ||
    !get_line_range(src_code, pos->line_number, &line_start, &line_end)
  ) {
    return;
  }
  const char* line_prefix = "> ";
  const int line_prefix_len = (const int) strlen(line_prefix);

  const char* line_start_str = src_code + line_start;
  int line_size = line_end - line_start;
  int col_size = pos->col_end - pos->col_start;

  // Print text that comes before the range
  printf("%s%.*s", line_prefix, pos->col_start, line_start_str);

  // Print the range: col_start .. col_end highlighted
  SET_CONSOLE_COLOR(ERROR_COLOR);
  printf("%.*s", col_size, line_start_str + pos->col_start);
  RESTORE_CONSOLE_COLOR;

  // Print the text that comes after the range
  printf("%.*s\n", line_size - pos->col_end, line_start_str + pos->col_start + col_size);


  printf("%*s", line_prefix_len + pos->col_start, " ");
  SET_CONSOLE_COLOR(ERROR_COLOR);
  printf("%.*s", (pos->col_end - pos->col_start), "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^...");
  RESTORE_CONSOLE_COLOR;
  printf("\n");

  RESTORE_CONSOLE_COLOR;
}


static void default_error_handler_fn(error_handler* err_handler, char* msg, src_pos* pos) {
  START_CONSOLE_COLOR;

  SET_CONSOLE_COLOR(ERROR_COLOR);
  printf("Error: ");
  RESTORE_CONSOLE_COLOR;

  printf("%s", msg);
  if (pos != NULL) {
    SET_CONSOLE_COLOR(POSITION_COLOR);
    printf(" (line: %d column: %d)\n", pos->line_number, pos->col_start);
    RESTORE_CONSOLE_COLOR;

    if (err_handler->src_code) {
      print_location(err_handler->src_code, pos);
    } else {
      printf("(source not available)\n");
    }
  } else {
    printf(" (position not available)\n");
  }

  RESTORE_CONSOLE_COLOR;
  exit(1);
}

error_handler* default_error_handler(const char* code) {
  error_handler* handler = (error_handler*) malloc(sizeof(error_handler));
  handler->handler_fn = default_error_handler_fn;
  handler->handler_data = NULL;
  handler->src_code = code;
  return handler;
}

#endif // __ERROR_H__