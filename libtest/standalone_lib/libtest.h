/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBTEST_IMPLEMENTATION before including this file:
 * #define LIBTEST_IMPLEMENTATION
 * #include "libtest.h"
 */

#ifndef LIBTEST_DEF
#ifdef __cplusplus
#define LIBTEST_DEF extern "C"
#else
#define LIBTEST_DEF extern
#endif
#endif

#ifndef LIBTEST_H
#define LIBTEST_H

#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#define TEST(suite_name, func_name)                                                         \
  static void test_ ## suite_name ## _ ## func_name(void);                                  \
  static void register_ ## suite_name ## _ ## func_name(void) __attribute__((constructor)); \
  static void register_ ## suite_name ## _ ## func_name(void) {                             \
    test_add(test_ ## suite_name ## _ ## func_name, #suite_name, #func_name);               \
  }                                                                                         \
  static void test_ ## suite_name ## _ ## func_name(void)


#define ASSERT_EQ(actual, expected) do {             \
    long long _act = (long long)(actual);            \
    long long _exp = (long long)(expected);          \
    if (_act != _exp) {                              \
        test_fail_formatted(__FILE__, __LINE__,      \
            "ASSERT_EQ(" #actual ", " #expected ")", \
            "Expected %lld, got %lld", _exp, _act);  \
        return; /* Esce dal test corrente */         \
    }                                                \
} while(0)

#define ASSERT_NE(actual, expected) do {             \
    long long _act = (long long)(actual);            \
    long long _exp = (long long)(expected);          \
    if (_act == _exp) {                              \
        test_fail_formatted(__FILE__, __LINE__,      \
            "ASSERT_NE(" #actual ", " #expected ")", \
            "Expected %lld, got %lld", _exp, _act);  \
        return; /* Esce dal test corrente */         \
    }                                                \
} while(0)

#define ASSERT_STR_EQ(actual, expected) do {                       \
  const char *_act = (const char *)(actual);                       \
  const char *_exp = (const char *)(expected);                     \
  if (_act != _exp &&                                              \
      (_act == NULL || _exp == NULL || strcmp(_act, _exp) != 0)) { \
    test_fail_formatted(__FILE__, __LINE__,                        \
        "ASSERT_STR_EQ(" #actual ", " #expected ")",               \
        "Expected %s, got %s",                                     \
        _exp ? _exp : "NULL", _act ? _act : "NULL");               \
    return; /* Esce dal test corrente */                           \
  }                                                                \
} while(0)

#define ASSERT_FLOAT_EQ(actual, expected, epsilon) do { \
  double _act = (double)(actual);                       \
  double _exp = (double)(expected);                     \
  double _eps = (double)(epsilon);                      \
  double _diff = _act - _exp;                           \
  if(_diff < 0) _diff = -_diff;                         \
  if (_diff > _eps) {                                   \
    test_fail_formatted(__FILE__, __LINE__,             \
        "ASSERT_FLOAT_EQ(" #actual ", " #expected ")",  \
        "Expected %f, got %f", _exp, _act);             \
      return; /* Esce dal test corrente */              \
  }                                                     \
} while(0)

#define ASSERT_TRUE(cond) do {                  \
    if (!(cond)) {                              \
        test_fail_formatted(__FILE__, __LINE__, \
            "ASSERT_TRUE(" #cond ")",           \
            "Expected true");                   \
        return; /* Esce dal test corrente */    \
    }                                           \
} while(0)

#define ASSERT_FALSE(cond) do {                 \
    if (cond) {                                 \
        test_fail_formatted(__FILE__, __LINE__, \
            "ASSERT_FALSE(" #cond ")",          \
            "Expected false");                  \
        return; /* Esce dal test corrente */    \
    }                                           \
} while(0)

#define EXPECT_EQ(actual, expected) do {             \
    long long _act = (long long)(actual);            \
    long long _exp = (long long)(expected);          \
    if (_act != _exp) {                              \
        test_expect_fail_formatted(__FILE__, __LINE__, \
            "EXPECT_EQ(" #actual ", " #expected ")", \
            "Expected %lld, got %lld", _exp, _act);  \
    }                                                \
} while(0)

#define EXPECT_NE(actual, expected) do {             \
    long long _act = (long long)(actual);            \
    long long _exp = (long long)(expected);          \
    if (_act == _exp) {                              \
        test_expect_fail_formatted(__FILE__, __LINE__, \
            "EXPECT_NE(" #actual ", " #expected ")", \
            "Expected %lld, got %lld", _exp, _act);  \
    }                                                \
} while(0)

#define EXPECT_STR_EQ(actual, expected) do {                       \
  const char *_act = (const char *)(actual);                       \
  const char *_exp = (const char *)(expected);                     \
  if (_act != _exp &&                                              \
      (_act == NULL || _exp == NULL || strcmp(_act, _exp) != 0)) { \
    test_expect_fail_formatted(__FILE__, __LINE__,                 \
        "EXPECT_STR_EQ(" #actual ", " #expected ")",               \
        "Expected %s, got %s",                                     \
        _exp ? _exp : "NULL", _act ? _act : "NULL");               \
  }                                                                \
} while(0)

#define EXPECT_FLOAT_EQ(actual, expected, epsilon) do { \
  double _act = (double)(actual);                       \
  double _exp = (double)(expected);                     \
  double _eps = (double)(epsilon);                      \
  double _diff = _act - _exp;                           \
  if(_diff < 0) _diff = -_diff;                         \
  if (_diff > _eps) {                                   \
    test_expect_fail_formatted(__FILE__, __LINE__,      \
        "EXPECT_FLOAT_EQ(" #actual ", " #expected ")",  \
        "Expected %f, got %f", _exp, _act);             \
  }                                                     \
} while(0)

#define EXPECT_TRUE(cond) do {                  \
    if (!(cond)) {                              \
        test_expect_fail_formatted(__FILE__, __LINE__, \
            "EXPECT_TRUE(" #cond ")",           \
            "Expected true");                   \
    }                                           \
} while(0)

#define EXPECT_FALSE(cond) do {                 \
    if (cond) {                                 \
        test_expect_fail_formatted(__FILE__, __LINE__, \
            "EXPECT_FALSE(" #cond ")",          \
            "Expected false");                  \
    }                                           \
} while(0)

typedef struct {
  void (*func)(void);
  const char *suite;
  const char *name;
} TestCase;

typedef struct TestContext {
  int total_tests;
  int capacity;
  TestCase *test_case;
  int passed_tests;
  int failed_tests;
  bool current_test_failed;
  char buffer_text_error[512];
} TestContext;

LIBTEST_DEF void test_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...);
LIBTEST_DEF void test_expect_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...);
LIBTEST_DEF void test_add(void (*func)(void), const char *suite, const char *name);
LIBTEST_DEF int test_run(void);

#endif

#ifdef LIBTEST_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>

static TestContext tc = {
  .total_tests = 0, 
  .capacity = 0, 
  .passed_tests = 0, 
  .failed_tests = 0,
  .current_test_failed = false,
  .test_case = NULL
};

LIBTEST_DEF void test_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...) {
    tc.current_test_failed = true;

    char msg_buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    snprintf(tc.buffer_text_error, sizeof(tc.buffer_text_error), "[%s:%d] %s -> %s", file, line, expr, msg_buffer);
}

LIBTEST_DEF void test_expect_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...) {
    tc.current_test_failed = true;

    char msg_buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    printf("  \033[31m[ FAILED ]\033[0m [%s:%d] %s -> %s\n", file, line, expr, msg_buffer);
}

LIBTEST_DEF void test_add(void (*func)(void), const char *suite, const char *name) {
  if (tc.total_tests >= tc.capacity) {
    if (tc.capacity == 0) {
      tc.capacity = 32;
    } else {
      tc.capacity *= 2;
    }
    TestCase *test = realloc(tc.test_case, tc.capacity * sizeof(TestCase));
    if (test == NULL) {
      free(tc.test_case);
      fprintf(stderr, "libtest: Out of memory during test registration\n");
      exit(1);
    }
    tc.test_case = test;
  }
  tc.test_case[tc.total_tests].func = func;
  tc.test_case[tc.total_tests].suite = suite;
  tc.test_case[tc.total_tests].name = name;
  tc.total_tests++;
}

LIBTEST_DEF int test_run(void) {
  for (int i = 0; i < tc.total_tests; i++) {
    printf("[ RUN ] %s.%s\n", tc.test_case[i].suite, tc.test_case[i].name);
    tc.current_test_failed = false;
    tc.buffer_text_error[0] = '\0';
    
    // Esecuzione del test
    tc.test_case[i].func();
    
    if (tc.current_test_failed) {
      tc.failed_tests++;
      if (tc.buffer_text_error[0] != '\0') {
        printf("\033[31m[ FAILED ]\033[0m %s\n", tc.buffer_text_error);
      } else {
        printf("\033[31m[ FAILED ]\033[0m\n");
      }
    } else {
      tc.passed_tests++;
      printf("\033[32m[ OK ]\033[0m\n");
    }
  }

  printf("\n==========================================\n");
  printf("PASSED: %d/%d\n", tc.passed_tests, tc.total_tests);
  
  int failed = tc.failed_tests;
  if (failed > 0) {
    printf("\033[31mFAILED: %d/%d\033[0m\n", failed, tc.total_tests);
  } else {
    printf("\033[32mALL TESTS PASSED!\033[0m\n");
  }
  printf("==========================================\n");

  free(tc.test_case);
  tc.test_case = NULL;
  tc.total_tests = 0;
  tc.capacity = 0;
  
  return failed;
}

#endif // LIBTEST_IMPLEMENTATION
