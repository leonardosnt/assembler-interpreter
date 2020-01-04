#ifndef __TEST_H__
#define __TEST_H__

#define TEST_MAX_NAME_SIZE 20
#define TEST_MAX_MESSAGE_SIZE 1000

typedef struct _test_status {
  int success;
  char message[TEST_MAX_MESSAGE_SIZE]; // TOOD: check for "overflow" MAX_MSG_SIZE
} _test_status;

#define METHOD_NAME_OF_TEST(test_name)  test_name##__df

// Test definition
#define DEF_TEST(test_name) \
  _test_status METHOD_NAME_OF_TEST(test_name)() { \
    _test_status __test;  \


#define END_TEST  \
  __test.success = 1; \
  sprintf(__test.message, "Passed!"); \
  return __test;  \
}

#define PASS() \
  do {  \
    __test.success = 1; \
    return __test; \
  } while (0);

// Assertion
#define FAILF(msg...) \
  do {  \
    int n = sprintf(__test.message, msg); \
    sprintf(__test.message + n, " | At %s:%d", __FILE__, __LINE__); \
    __test.success = 0; \
    return __test; \
  } while (0);

#define FAIL(msg) \
  do {  \
    int n = sprintf(__test.message, "%s", msg); \
    sprintf(__test.message + n, " | At %s:%d", __FILE__, __LINE__); \
    __test.success = 0; \
    return __test; \
  } while (0);

#define _assert_eq_base(a, b, val_type, print_fmt)                                              \
  do {                                                                                          \
    val_type a_val = a;                                                                         \
    val_type b_val = b;                                                                         \
    if (a_val != b_val) {                                                                       \
      FAILF("Assertion failed: '"#a" == "#b"'  ->  ("print_fmt" != "print_fmt")", a_val, b_val) \
    } \
  } while (0); \

#define ASSERT(expr) do { if (!(expr)) FAIL("Assertion failed: '"#expr"'"); } while (0);

#define ASSERT_EQI(a, b) _assert_eq_base(a, b, int, "%d")
#define ASSERT_EQF(a, b) _assert_eq_base(a, b, float, "%f")
#define ASSERT_EQC(a, b) _assert_eq_base(a, b, char, "'%c'")
#define ASSERT_EQS(a, b)  \
  do {                                                                                         \
    char* a_val = a;                                                                           \
    char* b_val = b;                                                                           \
    if (a_val != b_val && strcmp(a_val, b_val) != 0) {                                         \
      FAILF("Assertion failed: string contents differ: (\"%s\" != \"%s\")", a_val, b_val) \
    } \
  } while (0); \

#define ASSERT_EQ(a, b) ASSERT(a == b)
#define ASSERT_NOT_NULL(ptr) ASSERT(ptr != NULL)
#define ASSERT_NULL(ptr) ASSERT(ptr == NULL)

#ifdef TEST_USE_ANSI_COLOR
  #define SUCCESS_COLOR "\x1b[32m"
  #define FAIL_COLOR "\x1b[91m"
  #define ANSI_RESET "\x1b[0m"
#else
  #define SUCCESS_COLOR ""
  #define FAIL_COLOR ""
  #define ANSI_RESET ""
#endif

#define REPORT_ONLY_FAILS() report_only_fails = 1;

// Suite
#define SUITE_INIT(name)                    \
{                                           \
  printf("Running suite: '%s'\n", #name);   \
  int total = 0;                            \
  int success = 0;                          \
  int failed = 0;                           \
  int report_only_fails = 0;

#define ADD_TEST(test_name)                                           \
  {                                                                   \
    _test_status t = METHOD_NAME_OF_TEST(test_name)();                \
    total++;                                                          \
    if (t.success) {                                                  \
      if (!report_only_fails)                                         \
        printf(SUCCESS_COLOR "  "#test_name" [SUCCESS]\n" ANSI_RESET);\
      success++;                                                      \
    } else {                                                          \
      failed++;                                                       \
      printf(FAIL_COLOR "  "#test_name" [FAILED]\n");                 \
      printf("    Reason: %s\n" ANSI_RESET, t.message);               \
    }                                                                 \
  }

#define SUITE_RUN                                                                      \
    printf("Finished suite. Total %d, Succeed %d, Failed %d\n", total, success, failed);  \
  }

#endif // __TEST_H__