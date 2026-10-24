# libtest

A lightweight, zero-boilerplate, single-header unit testing framework for C.

`libtest` provides an expressive testing experience inspired by Google Test and Catch2, supporting both fatal (`ASSERT_*`) and non-fatal (`EXPECT_*`) assertions with automatic constructor-based test discovery and ANSI-colored terminal output.

---

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
- [ASSERT vs. EXPECT (Fatal vs. Non-Fatal)](#assert-vs-expect-fatal-vs-non-fatal)
- [Quick Start](#quick-start)
- [Assertions Reference](#assertions-reference)
  - [Equality & Comparison Checks](#equality--comparison-checks)
  - [String Checks](#string-checks)
  - [Floating-Point Checks](#floating-point-checks)
  - [Boolean Checks](#boolean-checks)
- [Test Organization](#test-organization)
- [CI / Exit Code Integration](#ci--exit-code-integration)
- [Compiler Compatibility](#compiler-compatibility)

---

## Features

- **Single-Header Library:** Pure C99, drop `libtest.h` into your project and you are ready.
- **Automatic Test Registration:** Tests register themselves automatically at startup using `__attribute__((constructor))`—no manual lists or test suite registration functions.
- **Dual Assertion System:**
  - `ASSERT_*`: Fatal assertions. Aborts the test immediately on failure to prevent crashes on invalid pointers.
  - `EXPECT_*`: Non-fatal assertions. Streams the failure directly to the terminal and continues running subsequent assertions.
- **Rich Failure Reporting:** Displays exact file paths, line numbers, failing expressions, expected values, and actual values.
- **Safe String Comparisons:** `ASSERT_STR_EQ` / `EXPECT_STR_EQ` safely handle `NULL` inputs without segmentation faults.
- **ANSI Color Output:** Clean, colorized terminal feedback for passes, warnings, and failures.
- **CI / Pipeline Ready:** `test_run()` returns the total number of failed tests, making it a drop-in exit code for automated CI/CD runners.

---

## How It Works

In **exactly one** `.c` file (usually your test runner file), define `LIBTEST_IMPLEMENTATION` before including `libtest.h`:

```c
#define LIBTEST_IMPLEMENTATION
#include "libtest.h"
```

In any other test source files, simply include the header normally:

```c
#include "libtest.h"
```

---

## ASSERT vs. EXPECT (Fatal vs. Non-Fatal)

| Type | Macro Prefix | On Failure | When to Use |
| :--- | :--- | :--- | :--- |
| **Fatal** | `ASSERT_*` | Records error and **aborts current test immediately** (`return;`). | When subsequent code cannot safely execute if the check fails (e.g., checking that a pointer is not `NULL` before dereferencing it). |
| **Non-Fatal** | `EXPECT_*` | Prints failure **immediately** to terminal and **continues execution**. | When checking multiple independent properties of an object or state, allowing you to see all failures in a single run. |

---

## Quick Start

Create a `test_main.c`:

```c
#include <stdio.h>

#define LIBTEST_IMPLEMENTATION
#include "libtest.h"

typedef struct {
    int width;
    int height;
} Rectangle;

Rectangle create_rect(int w, int h) {
    return (Rectangle){ .width = w, .height = h };
}

TEST(geometry, rectangle_properties) {
    Rectangle r = create_rect(10, 20);

    // Non-fatal checks: if width fails, height is still verified!
    EXPECT_EQ(r.width, 10);
    EXPECT_EQ(r.height, 20);
    EXPECT_TRUE(r.width > 0);
}

TEST(memory, pointer_safety) {
    char *buffer = malloc(32);

    // Fatal check: if allocation fails, abort before touching buffer!
    ASSERT_NE(buffer, NULL);

    buffer[0] = 'A';
    EXPECT_EQ(buffer[0], 'A');

    free(buffer);
}

int main(void) {
    return test_run();
}
```

### Compiling and Running
```bash
gcc -Wall -Wextra test_main.c -o run_tests
./run_tests
```

### Output Example (All Passing)
```text
[ RUN ] geometry.rectangle_properties
[ OK ]
[ RUN ] memory.pointer_safety
[ OK ]

==========================================
PASSED: 2/2
ALL TESTS PASSED!
==========================================
```

### Output Example (With `EXPECT` and `ASSERT` Failures)
```text
[ RUN ] geometry.rectangle_properties
  [ FAILED ] [test_main.c:19] EXPECT_EQ(r.width, 15) -> Expected 15, got 10
  [ FAILED ] [test_main.c:20] EXPECT_EQ(r.height, 25) -> Expected 25, got 20
[ FAILED ]
[ RUN ] memory.pointer_safety
[ FAILED ] [test_main.c:28] ASSERT_NE(buffer, NULL) -> Expected 0, got 0

==========================================
PASSED: 0/2
FAILED: 2/2
==========================================
```

---

## Assertions Reference

Every assertion comes in both `ASSERT_*` (fatal) and `EXPECT_*` (non-fatal) forms.

### Equality & Comparison Checks

| Fatal | Non-Fatal | Description |
| :--- | :--- | :--- |
| `ASSERT_EQ(act, exp)` | `EXPECT_EQ(act, exp)` | Checks `actual == expected`. Casts values internally to `long long`. |
| `ASSERT_NE(act, exp)` | `EXPECT_NE(act, exp)` | Checks `actual != expected`. Casts values internally to `long long`. |

```c
ASSERT_EQ(status_code, 200);
EXPECT_NE(file_descriptor, -1);
```

---

### String Checks

| Fatal | Non-Fatal | Description |
| :--- | :--- | :--- |
| `ASSERT_STR_EQ(act, exp)` | `EXPECT_STR_EQ(act, exp)` | Compares two null-terminated strings via `strcmp`. **NULL-safe**. |

- If both are `NULL`, the assertion **passes**.
- If one is `NULL` and the other is not, it reports a failure without crashing.

```c
EXPECT_STR_EQ(path_basename("/usr/bin/gcc"), "gcc");
ASSERT_STR_EQ(get_error_msg(), NULL);
```

---

### Floating-Point Checks

| Fatal | Non-Fatal | Description |
| :--- | :--- | :--- |
| `ASSERT_FLOAT_EQ(act, exp, eps)` | `EXPECT_FLOAT_EQ(act, exp, eps)` | Verifies `fabs(act - exp) <= eps`. Prevents precision rounding issues. |

```c
double val = 0.1 + 0.2;
EXPECT_FLOAT_EQ(val, 0.3, 0.00001);
```

---

### Boolean Checks

| Fatal | Non-Fatal | Description |
| :--- | :--- | :--- |
| `ASSERT_TRUE(condition)` | `EXPECT_TRUE(condition)` | Asserts that `condition` evaluates to non-zero (`true`). |
| `ASSERT_FALSE(condition)` | `EXPECT_FALSE(condition)` | Asserts that `condition` evaluates to zero (`false`). |

```c
ASSERT_TRUE(path_is_descendant_of("a/b/c", "a"));
EXPECT_FALSE(is_server_closed(server));
```

---

## Test Organization

Tests are declared with the `TEST(suite_name, test_name)` macro:

```c
TEST(parser_json, valid_object) {
    // ...
}

TEST(parser_json, trailing_comma) {
    // ...
}

TEST(http_client, timeout_handling) {
    // ...
}
```

- `suite_name`: Grouping identifier (module, feature, or component).
- `test_name`: Description of the specific test case.

The runner prints them in format `[ RUN ] suite_name.test_name`.

---

## CI / Exit Code Integration

`test_run()` automatically returns the total number of failed tests:
- Returns `0` if all tests passed.
- Returns `N` (where `N > 0`) if one or more tests failed.

This integrates natively with build systems and CI pipelines:

```c
int main(void) {
    return test_run();
}
```

In bash or CI scripts:
```bash
./run_tests && echo "Build Succeeded" || exit 1
```

---

## Compiler Compatibility

`libtest` uses `__attribute__((constructor))` to register tests before `main()` executes. Supported out-of-the-box by:
- **GCC** (Linux, Windows/MinGW)
- **Clang** (macOS, Linux, Windows)
- Any C99 compiler supporting GNU constructor extensions.
