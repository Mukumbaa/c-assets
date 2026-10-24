#include "libtest.h"
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

extern void test_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...) {
    tc.current_test_failed = true;

    char msg_buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    snprintf(tc.buffer_text_error, sizeof(tc.buffer_text_error), "[%s:%d] %s -> %s", file, line, expr, msg_buffer);
}

extern void test_expect_fail_formatted(const char *file, int line, const char *expr, const char *fmt, ...) {
    tc.current_test_failed = true;

    char msg_buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buffer, sizeof(msg_buffer), fmt, args);
    va_end(args);

    printf("  \033[31m[ FAILED ]\033[0m [%s:%d] %s -> %s\n", file, line, expr, msg_buffer);
}

extern void test_add(void (*func)(void), const char *suite, const char *name) {
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

extern int test_run(void) {
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
