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

extern void test_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...);
extern void test_expect_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...);
extern void test_add(void (*func)(void), const char *suite, const char *name);
extern int test_run(void);

#endif

