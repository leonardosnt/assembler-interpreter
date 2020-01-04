#ifndef __ALLOC_SPY_H__
#define __ALLOC_SPY_H__

//#define ALLOC_SPY

#include <stdio.h>
#include "stretchy_buffer.h"

#ifdef ALLOC_SPY

typedef struct allocation {
  const char* size_expr;
  size_t size;
  int line;
  int count;
  enum type {
    ALLOC_TYPE_MALLOC,
    ALLOC_TYPE_CALLOC
  } type;
} allocation;

typedef struct allocation_file {
  const char* name;
  size_t total;
  allocation** allocations;
} allocation_file;

static allocation_file** allocation_files = NULL;
static size_t total_allocated = 0;

static allocation_file* get_allocation_file(const char* name) {
  // TODO: hash lookup
  for (int i = 0, n = sb_count(allocation_files); i < n; i++) {
    allocation_file* f = allocation_files[i];
    if (f->name == name) {
      return f;
    }
  }
  return NULL;
}

// This will not work if the same line contains more than one malloc
static allocation* get_allocation_by_line(allocation_file* af, int line) {
  // TODO: hash lookup
  for (int i = 0, n = sb_count(af->allocations); i < n; i++) {
    allocation* al = af->allocations[i];
    if (al->line == line) {
      return al;
    }
  }
  return NULL;
}

void alloc_spy_report() {
  printf("## AllocSpy Report (total: %I64d) {\n", total_allocated);
  for (int i = 0, n = sb_count(allocation_files); i < n; i++) {
    allocation_file* af = allocation_files[i];
    printf("  %s (total: %I64d) {\n", af->name, af->total);
    for (int j = 0, n2 = sb_count(af->allocations); j < n2; j++) {
      allocation* al = af->allocations[j];
      printf("   line %4d: malloc(%s) | size=%I64d | count=%d | total %I64d bytes\n",
        al->line, al->size_expr, al->size, al->count, al->size * al->count);
    }
    printf("  }\n");
  }
  printf("}\n");
}

void freeWrapper(void* ptr, const char* file, int line) {
  printf("[%s:%d] free(%p)\n", file, line, ptr);
  free(ptr);
}

void* mallocWrapper(size_t sz, const char* file, int line, const char* sz_expr)  {
  {
    allocation_file* af = get_allocation_file(file);
    if (af == NULL) {
      af = (allocation_file*) malloc(sizeof(allocation_file));
      af->name = file;
      af->total = 0;
      af->allocations = NULL;
      sb_push(allocation_files, af);
    }

    allocation* al = get_allocation_by_line(af, line);
    if (al == NULL) {
      al = (allocation*) malloc(sizeof(allocation));
      al->line = line;
      al->count = 0;
      al->size = sz;
      al->size_expr = sz_expr;
      al->type = ALLOC_TYPE_MALLOC;

      sb_push(af->allocations, al);
    }
    al->count += 1;
    af->total += sz;
    total_allocated += sz;
  }

  void* data = malloc(sz);
  //printf("[%s:%d] Allocated %I64d bytes: malloc(%s)\n", file, line, sz, sz_expr);
  return data;
}

void* callocWrapper(size_t num, size_t sz, const char* file, int line,
                    const char* num_expr, const char* sz_expr)  {
  void* data = calloc(num, sz);
  printf("TODO [%s:%d] Allocated %I64d bytes: calloc(%s)\n", file, line, sz, sz_expr);
  return data;
}

#define free(ptr) freeWrapper(ptr, __FILE__, __LINE__)
#define malloc(size) (mallocWrapper(size, __FILE__, __LINE__, #size))
#define calloc(num, size) (callocWrapper(num, size, __FILE__, __LINE__, #num, #size))
#else
void alloc_spy_report() {
  // Do nothing
}
#endif // ALLOC_SPY

#endif // __ALLOC_SPY_H__