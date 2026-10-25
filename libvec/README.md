# libvec

A lightweight, robust, single-header dynamic array (vector) library written in standard C99.

`libvec` provides a contiguous, generic dynamic array that stores elements **by value** (`void*` buffer with an explicit `item_size`), ensuring cache locality and zero individual heap allocations per element. It features geometric buffer expansion ($2\times$), integer overflow protection on arithmetic operations, and $O(1)$ fast deletions.

---

## Table of Contents

- [Features](#features)
- [Design Philosophy](#design-philosophy)
- [Quick Start](#quick-start)
- [Data Structures & Constants](#data-structures--constants)
- [API Reference](#api-reference)
  - [Initialization & Destruction](#1-initialization--destruction)
  - [Capacity & Buffer Sizing](#2-capacity--buffer-sizing)
  - [Insertion & Appending](#3-insertion--appending)
  - [Removal & Extraction](#4-removal--extraction)
  - [Element Access & Inspection](#5-element-access--inspection)
  - [Linear Search](#6-linear-search)
- [Performance & Memory Safety](#performance--memory-safety)
- [Running Unit Tests](#running-unit-tests)
- [License](#license)

---

## Features

- **Single-Header (`libvec.h`):** Zero external dependencies; requires only standard C headers (`stddef.h`, `stdint.h`, `stdlib.h`, `string.h`).
- **Contiguous Value Storage:** Elements are packed sequentially into a single heap buffer, minimizing cache misses.
- **Generic Data Types:** Stores any C type (`int`, `double`, pointers, or custom structs) via byte-level size tracking (`item_size`).
- **Overflow-Safe Memory Math:** Guarded against `SIZE_MAX` arithmetic wrap-arounds during capacity reallocation and offset calculations.
- **Fast Unordered Removal:** Implements `vec_swap_remove` for instant $O(1)$ deletions when order preservation is not needed.
- **Fail-Safe Sizing:** Clean handling of edge cases such as `shrink_to_fit` on empty vectors without invoking undefined behaviors on `realloc(..., 0)`.

---

## Design Philosophy

In C, generic containers are often implemented in one of two ways:
1. **Pointer collections (`void*` items):** Every item requires an independent `malloc`, causing memory fragmentation and multiple cache misses.
2. **Contiguous byte buffers (By-Value):** All elements are stored adjacent in one contiguous buffer.

`libvec` chooses the **contiguous by-value** approach. By providing `item_size` at initialization:
- Adding primitive types or structs copies the bytes directly into the buffer via `memcpy`.
- Iterating elements is extremely fast due to hardware prefetching and CPU cache locality.
- Memory management is simplified: calling `vec_free` frees the entire array in a single operation.

---

## Quick Start

### 1. Inclusion
In **exactly one** `.c` file, define `LIBVEC_IMPLEMENTATION` before including `libvec.h`:

```c
#define LIBVEC_IMPLEMENTATION
#include "libvec.h"
```

In any other translation unit, include it normally:
```c
#include "libvec.h"
```

### 2. Basic Example

```c
#include <stdio.h>

#define LIBVEC_IMPLEMENTATION
#include "libvec.h"

typedef struct {
    int id;
    char name[32];
} User;

int main(void) {
    vector v;
    vec_init(&v, sizeof(User));

    User u1 = { .id = 1, .name = "Alice" };
    User u2 = { .id = 2, .name = "Bob" };

    vec_append(&v, &u1);
    vec_append(&v, &u2);

    for (size_t i = 0; i < vec_len(&v); i++) {
        User *u = (User *)vec_get(&v, i);
        printf("[%zu] ID: %d, Name: %s\n", i, u->id, u->name);
    }

    vec_free(&v);
    return 0;
}
```

---

## Data Structures & Constants

### `vector`
```c
typedef struct {
    size_t len;        // Number of active elements currently stored
    size_t capacity;   // Total elements that can be stored without reallocating
    size_t item_size;  // Size in bytes of a single element
    void *values;      // Pointer to the contiguous raw memory buffer
} vector;
```

### Constants
- `VEC_BASIC_CAP (32)`: The default initial capacity assigned during `vec_init`.

---

## API Reference

### 1. Initialization & Destruction

#### `int vec_init(vector *vec, size_t item_size)`
Initializes a new vector with default initial capacity (`VEC_BASIC_CAP`).
- **Parameters:**
  - `vec`: Pointer to the `vector` structure to initialize.
  - `item_size`: Size in bytes of each element (e.g. `sizeof(int)`).
- **Returns:** `0` on success, `1` if `vec == NULL`, `item_size == 0`, or allocation fails.

#### `int vec_init_wcapacity(vector *vec, size_t item_size, size_t capacity)`
Initializes a vector with a user-defined initial capacity.
- **Parameters:**
  - `capacity`: Number of elements to pre-allocate.
- **Returns:** `0` on success, `1` on error or allocation failure.

#### `void vec_free(vector *vec)`
Frees the internal buffer allocated on the heap and resets `values = NULL`, `len = 0`, and `capacity = 0`. Safe to call on stack-allocated `vector` variables.
- **Parameters:** `vec`: Pointer to the vector to release.

#### `void vec_clear(vector *vec)`
Logically empties the vector by resetting `len = 0`. Keeps the allocated buffer and capacity intact for subsequent reuse at zero allocation cost.
- **Parameters:** `vec`: Pointer to the vector.

---

### 2. Capacity & Buffer Sizing

#### `int vec_reserve(vector *vec, size_t space)`
Ensures that the vector has room for at least `space` **additional** elements beyond its current length (`capacity >= len + space`).
- **Growth Strategy:** Automatically doubles capacity ($2\times$) until the required capacity is met.
- **Returns:** `0` on success, `1` on arithmetic overflow or memory allocation failure.

#### `int vec_shrink_to_fit(vector *vec)`
Reduces allocated capacity to match the exact current length (`capacity == len`). If `len == 0`, the memory buffer is completely freed and reset to `NULL`.
- **Returns:** `0` on success, `1` on allocation failure.

---

### 3. Insertion & Appending

#### `int vec_append(vector *vec, const void *value)`
Copies `item_size` bytes from `value` into the end of the vector. Grows capacity automatically if full.
- **Parameters:**
  - `vec`: Target vector.
  - `value`: Pointer to the memory block containing the element to append.
- **Returns:** `0` on success, `1` if parameters are `NULL` or reallocation fails.

#### `int vec_insert_at(vector *vec, const void *value, size_t index)`
Inserts an element at a specific index. Shifts subsequent elements right by one slot using `memmove`. If `index == vec->len`, it performs an append.
- **Complexity:** $O(N)$ due to element shifting.
- **Parameters:**
  - `index`: Position where `value` should be placed (`0 <= index <= vec->len`).
- **Returns:** `0` on success, `1` if `index > vec->len` or allocation fails.

---

### 4. Removal & Extraction

#### `int vec_pop(vector *vec, void *out)`
Removes the last element from the vector in $O(1)$ time.
- **Parameters:**
  - `out`: Optional destination pointer where the removed element will be copied. Pass `NULL` to discard.
- **Returns:** `0` on success, `1` if `vec == NULL` or the vector is empty (`len == 0`).

#### `int vec_remove_at(vector *vec, size_t index, void *out)`
Removes the element at `index` and shifts all trailing elements left to fill the gap, preserving sequence order.
- **Complexity:** $O(N)$.
- **Parameters:**
  - `out`: Optional destination buffer for the removed element (can be `NULL`).
- **Returns:** `0` on success, `1` if `index >= vec->len`.

#### `int vec_swap_remove(vector *vec, size_t index, void *out)`
Removes the element at `index` by copying the **last element** of the vector into `index`, then decrementing length.
- **Complexity:** $O(1)$ instant removal.
- **Trade-off:** Does not preserve element ordering.
- **Returns:** `0` on success, `1` if `index >= vec->len`.

---

### 5. Element Access & Inspection

#### `void *vec_get(const vector *vec, size_t index)`
Retrieves a pointer to the element at the specified index.
- **Returns:** Pointer to the element, or `NULL` if `vec == NULL` or `index >= vec->len`.

#### `void *vec_first(const vector *vec)`
Returns a pointer to the first element (`index == 0`).
- **Returns:** Pointer to the first element, or `NULL` if empty.

#### `void *vec_last(const vector *vec)`
Returns a pointer to the last element (`index == len - 1`).
- **Returns:** Pointer to the last element, or `NULL` if empty.

#### `void *vec_data(vector *vec)`
Returns the raw pointer to the underlying continuous memory buffer (`vec->values`). Useful for passing vector contents to standard C APIs (e.g. `qsort`, `fwrite`).

#### `size_t vec_len(const vector *vec)`
Returns the number of elements currently stored in the vector. Returns `0` if `vec == NULL`.

#### `size_t vec_capacity(const vector *vec)`
Returns the total capacity currently allocated. Returns `0` if `vec == NULL`.

#### `int vec_is_empty(const vector *vec)`
Checks if the vector has no elements.
- **Returns:** `1` if `vec == NULL` or `vec->len == 0`, otherwise `0`.

#### Type-Casting Helper Macros
To avoid writing verbose manual pointer casts `(MyType *)vec_get(...)`, `libvec` provides ergonomic helper macros:

```c
#define vec_get_type(vec, index, type) ((type *)vec_get((vec), (index)))
#define vec_data_type(vec, type) ((type *)vec_data((vec)))
vector v;
vec_init(&v, sizeof(double));

double x = 3.14;
vec_append(&v, &x);

// Direct typed access without manual casting:
double *val = vec_get_type(&v, 0, double);
printf("Value: %f\n", *val);

// Access raw typed buffer:
double *raw_array = vec_data_type(&v, double);

```
---

### 6. Linear Search

#### `int vec_contains(const vector *vec, const void *value)`
Performs a linear scan over all elements comparing byte sequences via `memcmp`.
- **Returns:** `1` if an identical match is found, otherwise `0`.

---

## Performance & Memory Safety

1. **Stack Allocations Safe:** `vec_free(&v)` deallocates the internal buffer `v.values` without calling `free(vec)`, making it completely safe to declare vectors on the stack.
2. **Aliasing Safe Removal:** `vec_remove_at` uses `memmove` rather than `memcpy` to ensure source and destination overlaps do not cause data corruption.
3. **Safe Downsizing:** `vec_shrink_to_fit` explicitly frees the buffer when `len == 0` instead of calling `realloc(ptr, 0)`, preventing implementation-defined behaviors across standard library versions.
4. **Geometric Expansion:** Capacity doubles when full, delivering amortized $O(1)$ append times.

---

## Running Unit Tests

`libvec` is tested with `libtest.h`:

```bash
gcc -Wall -Wextra main.c -o test_runner
./test_runner
```

Output:
```text
Start tests

[ RUN ] VectorInitFree.DefaultInit
[ OK ]
[ RUN ] VectorInitFree.CustomCapacity
[ OK ]
[ RUN ] VectorAppendPop.BasicPushPop
[ OK ]
[ RUN ] VectorInsert.InsertAtPositions
[ OK ]
[ RUN ] VectorMemoryAndCapacity.ClearAndShrink
[ OK ]
[ RUN ] VectorSearch.ContainsPrimitive
[ OK ]
[ RUN ] VectorComplexType.StructHandling
[ OK ]

==========================================
PASSED: 11/11
ALL TESTS PASSED!
==========================================
```
