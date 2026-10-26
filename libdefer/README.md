# libdefer

A lightweight, zero-dependency, single-header resource cleanup and error-rollback library for C.

`libdefer` brings modern lexical resource management inspired by Go and Zig (`defer` and `errdefer`) to C. It guarantees deterministic LIFO (Last-In, First-Out) resource cleanup upon scope exit, eliminates cascading `goto cleanup` ladders, and prevents resource leaks natively across both GCC and Clang without requiring compiler extensions, external runtimes, or heap allocations.

---

## Table of Contents

- [Features](#features)
- [How to Include (Single-Header Pattern)](#how-to-include-single-header-pattern)
- [Core Concepts & Mechanics](#core-concepts--mechanics)
  - [The Defer Pattern (Go-style)](#the-defer-pattern-go-style)
  - [The Error-Defer Pattern (Zig-style)](#the-error-defer-pattern-zig-style)
  - [The LIFO Execution Guarantee](#the-lifo-execution-guarantee)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Unconditional Cleanup](#1-unconditional-cleanup)
  - [Conditional Error Cleanup](#2-conditional-error-cleanup)
  - [Success Exit Helper](#3-success-exit-helper)
- [Architecture & Design Details](#architecture--design-details)
  - [The Trampoline Entry Model (`_defer_entry`)](#the-trampoline-entry-model-_defer_entry)
  - [Error Flag Tracking & Pointer Indirection](#error-flag-tracking--pointer-indirection)
  - [Macro Hygiene & Two-Stage Token Concatenation](#macro-hygiene--two-stage-token-concatenation)
  - [Scope Lifetime (`{}`) vs Function Lifetime](#scope-lifetime--vs-function-lifetime)
  - [Zero Heap Allocation & Optimizer Friendliness](#zero-heap-allocation--optimizer-friendliness)
- [Real-World Example: Safe File Loader](#real-world-example-safe-file-loader)
- [Compiler Support & Prerequisites](#compiler-support--prerequisites)

---

## Features

- **Single-Header Implementation:** Header-only design ready to drop into any project with zero external dependencies.
- **Universal Compiler Portability:** Native out-of-the-box support for both **GCC** and **Clang** with standard compilation commands (no special flags required).
- **Zero Language-Server Noise:** 100% compliant with `clangd` out of the box without requiring custom `compile_flags.txt` configuration.
- **Deterministic LIFO Order:** Cleanups execute strictly in reverse order of declaration, preserving dependent resource hierarchies.
- **Dual Cleanup Primitives:**
  - `defer(fn, arg)`: Unconditional teardown (always executes on scope exit).
  - `errdefer(fn, arg)`: Conditional rollback (executes only when exiting through failure paths).
- **POSIX Cleanup Compatibility:** Uses the canonical `void (*)(void *)` trampoline pattern identical to `pthread_cleanup_push`.
- **Zero Runtime Memory Overhead:** Allocates small tracking records strictly on the stack; no `malloc`, no dynamic lists, and no runtime registries.
- **Variadic Return Support:** `defer_return(...)` seamlessly handles values, compound literals, and `void` returns with automatic state resolution.

---

## How to Include (Single-Header Pattern)

`libdefer` is a pure macro-and-inline header library. Simply include it in any source file:

```c
#include "libdefer.h"
```

No implementation macro definition (`#define LIBDEFER_IMPLEMENTATION`) is needed because `libdefer` consists entirely of lightweight `static inline` functions and preprocessor definitions.

---

## Core Concepts & Mechanics

### The Defer Pattern (Go-style)
In classical C, resource acquisition and resource release are separated in time and space: an object created at line 10 must be freed at line 120. If multiple early returns exist, cleanups must either be duplicated at every exit or consolidated through labeled jumps.

With `defer`, resource release logic is declared immediately adjacent to allocation:
```c
FILE *f = fopen("data.bin", "rb");
if (!f) return -1;
defer(fclose, f); // Bound immediately; guaranteed to run on any exit
```

### The Error-Defer Pattern (Zig-style)
When a function constructs an object incrementally, intermediate resources must only be deallocated if a subsequent step fails. If the function succeeds, the constructed object must be preserved and returned to the caller.

`errdefer` executes its registered cleanup **only** if the scope exits prematurely before `defer_return(...)` is reached.

### The LIFO Execution Guarantee
Resources often depend on preceding allocations (e.g., allocating a buffer inside a critical section requires freeing the buffer *before* releasing the mutex). `libdefer` guarantees that all deferred blocks run strictly in **Last-In, First-Out (LIFO)** order.

```text
Acquisition:    Alloc A  ───>  Lock B  ───>  Alloc C
Teardown:       Free C   ───>  Unlock B ───>  Free A
```

---

## Quick Start

```c
#include <stdio.h>
#include <stdlib.h>
#include "libdefer.h"

int run_task(void) {
    char *buffer = malloc(512);
    if (!buffer) return -1;
    defer(free, buffer); // Guaranteed to execute on ANY return

    FILE *f = fopen("log.txt", "w");
    if (!f) return -1; // buffer is freed automatically!
    defer(fclose, f);  // Guaranteed to execute before buffer is freed

    fprintf(f, "Processing completed.\n");
    return 0; // f is closed, then buffer is freed
}
```

---

## API Reference

### 1. Unconditional Cleanup

#### `defer(fn, arg)`
Registers a cleanup function `fn` to be invoked with `arg` when control leaves the current enclosing lexical scope (`{ ... }`), whether via normal exit, `return`, `break`, or `goto`.
- **Parameters:**
  - `fn` – Function pointer conforming to `void (*)(void *)` or castable to it (e.g., `free`, `fclose`).
  - `arg` – Target pointer or context passed directly to `fn`.
- **Execution Order:** Strictly LIFO relative to other `defer` and active `errdefer` entries in the same scope.

---

### 2. Conditional Error Cleanup

#### `DEFER_ERR_INIT`
Initializes the error-tracking context in the current scope:
```c
volatile int _defer_err_state = 1;
```
Must be invoked once at the beginning of any function or block that uses `errdefer`.
- **Behavior:** Assumes an error condition (`1`) by default until disarmed by `defer_return(...)`.

#### `errdefer(fn, arg)`
Registers a cleanup function `fn` to be executed on scope exit **only if** the error state is still active (`_defer_err_state != 0`).
- **Parameters:**
  - `fn` – Rollback function pointer (e.g., `free`, custom destructor).
  - `arg` – Target pointer or context passed to `fn`.
- **Behavior:** If execution reaches `defer_return(...)`, `_defer_err_state` is set to `0`, causing all registered `errdefer` handlers to be skipped during scope teardown.

---

### 3. Success Exit Helper

#### `defer_return(...)`
Disarms all active `errdefer` guards by setting `_defer_err_state = 0` and immediately executes a `return` statement.
- **Parameters:** Optional return expression (e.g., `defer_return(ptr)`, `defer_return(0)`, or empty `defer_return()` for `void` functions).
- **Behavior:**
  - Evaluates arguments (if provided).
  - Clears `_defer_err_state = 0`.
  - Normal `defer` statements **still execute**.
  - All `errdefer` statements are **skipped**.

---

## Architecture & Design Details

### The Trampoline Entry Model (`_defer_entry`)
Rather than relying on proprietary closures or nested function extensions, `libdefer` employs a universal trampoline pattern:
```c
typedef struct {
    _defer_fn_t fn;
    void *arg;
} _defer_entry;
```
Each call to `defer(fn, arg)` instantiates a small `_defer_entry` on the stack bound to `__attribute__((cleanup(_defer_run)))`. When the stack frame unwinds, the compiler invokes `_defer_run(&entry)`, which calls `entry.fn(entry.arg)`.

### Error Flag Tracking & Pointer Indirection
`errdefer` instances hold a direct pointer to the scope's error flag:
```c
typedef struct {
    _defer_fn_t fn;
    void *arg;
    const volatile int *err_state;
} _errdefer_entry;
```
When `defer_return(...)` clears `_defer_err_state = 0`, the mutated value is immediately visible to all `_errdefer_entry` instances via pointer dereference (`*err_state`), completely bypassing compiler constant-folding or stale value propagation issues.

### Macro Hygiene & Two-Stage Token Concatenation
The C preprocessor disables macro expansion for operands directly joined by `##`. `libdefer` uses two-stage indirection:
```c
#define DEFER_CONCAT_IMPL(a, b) a ## b
#define DEFER_CONCAT(a, b) DEFER_CONCAT_IMPL(a, b)
```
Passing `__COUNTER__` into internal variable names ensures collision-free symbols across multiple invocations within the same scope.

### Scope Lifetime (`{}`) vs Function Lifetime
Variables decorated with `__attribute__((cleanup))` are bound to their **nearest enclosing block (`{ ... }`)**, not strictly to the whole function:

```c
void example(void) {
    if (condition) {
        char *tmp = malloc(64);
        defer(free, tmp);
        // ...
    } // <--- defer(free, tmp) RUNS HERE at block exit!
}
```
This enables localized resource lifetimes inside loops, branches, or arbitrary compound blocks.

### Zero Heap Allocation & Optimizer Friendliness
Because tracking structs live on the stack:
- No heap memory (`malloc`) is touched.
- In optimized builds (`-O2` / `-O3`), modern compilers flatten the trampoline functions and inline the cleanup calls directly into the function exit paths, generating assembly equivalent to hand-crafted `goto cleanup` routines.

---

## Real-World Example: Safe File Loader

A concise, leak-free routine that loads an entire file into memory:

```c
#include <stdio.h>
#include <stdlib.h>
#include "libdefer.h"

char *load_file_contents(const char *path) {
    DEFER_ERR_INIT; // 1. Initialize error-tracking scope

    // 2. Open file; always close on exit
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    defer(fclose, f);

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) return NULL;

    // 3. Allocate buffer; roll back ONLY if reading fails
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) return NULL;
    errdefer(free, buffer);

    if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
        // Read error:
        // -> errdefer frees buffer
        // -> defer closes f
        return NULL;
    }
    buffer[size] = '\0';

    // 4. Success:
    // -> f is closed
    // -> errdefer is disarmed: buffer is preserved and returned!
    defer_return(buffer);
}

int main(void) {
    char *text = load_file_contents("example.txt");
    if (text) {
        printf("Loaded: %s\n", text);
        free(text);
    }
    return 0;
}
```

---

## Compiler Support & Prerequisites

`libdefer` requires a C compiler supporting the standard GNU C cleanup variable attribute (`__attribute__((cleanup(...)))`).

| Compiler Family | Support Status | Required Flags |
| :--- | :---: | :--- |
| **GCC** (Linux, MinGW, BSD) | **Full / Native** | *None* (`gcc -Wall main.c`) |
| **Clang** (Linux, macOS) | **Full / Native** | *None* (`clang -Wall main.c`) |
| **clang-cl** (Windows) | **Full / Native** | *None* |
| **MSVC** (`cl.exe`) | **Unsupported** | Lacks standard cleanup attribute support |

