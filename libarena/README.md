# libarena

A lightweight, robust, zero-dependency single-header arena (bump/linear) memory allocator written in standard C99.

`libarena` provides an efficient, region-based memory management strategy. It eliminates memory fragmentation, overhead from individual `malloc`/`free` calls, and common memory leak bugs by letting you allocate sequentially and free or rollback memory in bulk in $O(1)$ time.

---

## Table of Contents

- [Features](#features)
- [How to Include](#how-to-include)
- [Data Structures & Types](#data-structures--types)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Initialization & Destruction](#1-initialization--destruction)
  - [Memory Allocation](#2-memory-allocation)
  - [Macros](#3-convenience-macros)
  - [State & Checkpoint Management (Rollback)](#4-state--checkpoint-management-rollback)
  - [Scoped Scratchpad (TempArena)](#5-scoped-scratchpad-temparena)
  - [Duplication Utilities](#6-duplication-utilities)
  - [Telemetry & Inspection](#7-telemetry--inspection)
- [Design Details & Guarantees](#design-details--guarantees)
- [License](#license)

---

## Features

- **Single-Header Library:** Easy integration into any build system by defining `LIBARENA_IMPLEMENTATION` in one `.c` file.
- **Hardware-Enforced Alignment:** Ensures allocations meet CPU alignment constraints (defaults to 16 bytes, safe for SIMD and 64-bit pointers), with support for arbitrary power-of-two custom alignments.
- **Safe from Use-After-Free / Leaks:** Memory can be wiped in $O(1)$ with `arena_reset` or rolled back to an earlier state via checkpoints.
- **Heap & Non-Heap Backing:** Supports both dynamic heap memory (`malloc`) and caller-provided buffers (e.g. stack buffers or static storage) with automatic ownership tracking (`can_free`).
- **Scoped Temporary Allocator:** `TempArena` pattern guarantees paired save/restore checkpoints to prevent marker mismatches.
- **Overflow-Protected:** Arithmetic boundary checks prevent unsigned integer wrapping on allocation calculations.

---

## How to Include

`libarena` is a single-header library. Include `libarena.h` normally in any header or translation unit. In **exactly one** source file (e.g. `main.c`), define `LIBARENA_IMPLEMENTATION` before including:

```c
#define LIBARENA_IMPLEMENTATION
#include "libarena.h"
```

---

## Data Structures & Types

### `Arena`
The primary allocator container.
```c
typedef struct {
    uint8_t *buffer;   // Pointer to raw contiguous memory
    size_t capacity;   // Total size in bytes
    size_t offset;     // Index of the next available byte
    int can_free;      // 1 if allocated via malloc, 0 if using external buffer
} Arena;
```

### `ArenaMarker`
A lightweight snapshot type representing an offset bookmark.
```c
typedef size_t ArenaMarker;
```

### `TempArena`
A scoped checkpoint tracker that pairs an arena with a specific marker.
```c
typedef struct {
    Arena *arena;
    ArenaMarker marker;
} TempArena;
```

---

## Quick Start

```c
#include <stdio.h>

#define LIBARENA_IMPLEMENTATION
#include "libarena.h"

typedef struct {
    float x, y, z;
} Vector3;

int main(void) {
    Arena arena;
    if (arena_init(&arena, ARENA_MB(1)) != 0) {
        fprintf(stderr, "Failed to initialize arena\n");
        return 1;
    }

    // Allocate single structs and arrays with type safety
    Vector3 *pos = arena_alloc_type(&arena, Vector3);
    pos->x = 10.0f; pos->y = 20.0f; pos->z = 30.0f;

    int *numbers = arena_alloc_buffer(&arena, int, 100);
    numbers[0] = 42;

    // Duplicate a string into the arena
    char *copy = arena_dup_str(&arena, "Hello from libarena!");
    printf("%s (Used: %zu / %zu bytes)\n", copy, arena_used_bytes(&arena), arena_capacity(&arena));

    // Free the entire arena at once
    arena_free(&arena);
    return 0;
}
```

---

## API Reference

### 1. Initialization & Destruction

#### `int arena_init(Arena *arena, size_t capacity)`
Allocates a dynamic backing buffer using `malloc`.
- **Parameters:**
  - `arena`: Pointer to the uninitialized `Arena` struct.
  - `capacity`: Total byte capacity to allocate (use `ARENA_KB(n)` or `ARENA_MB(n)` helpers).
- **Returns:** `0` on success, `1` if `arena == NULL`, `capacity == 0`, or memory allocation failed.

#### `int arena_init_with_buffer(Arena *arena, uint8_t buffer[], size_t capacity)`
Initializes an arena using an existing memory buffer (e.g. allocated on the stack or in static data). The arena will not attempt to free this buffer upon `arena_free`.
- **Parameters:**
  - `arena`: Pointer to the uninitialized `Arena` struct.
  - `buffer`: Pre-allocated memory block.
  - `capacity`: Total size of the provided memory block.
- **Returns:** `0` on success, `1` on invalid parameters.

#### `void arena_free(Arena *arena)`
Releases the underlying heap buffer (if owned) and zeroes out all fields. Safe to call on stack-backed arenas (will zero fields without calling `free`).
- **Parameters:**
  - `arena`: Pointer to the `Arena` struct.

---

### 2. Memory Allocation

#### `void *arena_alloc(Arena *arena, size_t size)`
Allocates `size` bytes aligned to the default platform boundary (`ALIGNMENT = 16`).
- **Returns:** An aligned pointer to the allocated memory, or `NULL` if insufficient capacity or invalid input.

#### `void *arena_alloc_zero(Arena *arena, size_t size)`
Allocates `size` bytes (aligned to 16 bytes) and zeroes all allocated memory using `memset`.
- **Returns:** An aligned pointer with zeroed content, or `NULL` on failure.

#### `void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment)`
The core allocation routine. Allocates `size` bytes aligned to the specified `alignment` boundary.
- **Parameters:**
  - `arena`: Target arena.
  - `size`: Number of bytes requested.
  - `alignment`: Desired alignment in bytes. **Must be a non-zero power of two** (e.g. 1, 2, 4, 8, 16, 32, 64).
- **Returns:** A pointer satisfying the alignment constraint, or `NULL` if capacity is exceeded or parameters are invalid.

---

### 3. Convenience Macros

#### `arena_alloc_type(arena, type)`
Allocates storage for a single instance of `type` and automatically casts the result.
```c
MyStruct *s = arena_alloc_type(&arena, MyStruct);
```

#### `arena_alloc_buffer(arena, type, size)`
Allocates an array of `size` elements of `type`. Includes built-in multiplication overflow detection (`size <= SIZE_MAX / sizeof(type)`).
```c
int *arr = arena_alloc_buffer(&arena, int, 256);
```

---

### 4. State & Checkpoint Management (Rollback)

#### `void arena_reset(Arena *arena)`
Resets `offset` to `0` in $O(1)$ time. Does not release or wipe the backing memory; future allocations overwrite previous data.

#### `ArenaMarker arena_save(const Arena *arena)`
Captures the current allocation offset as a bookmark for subsequent rollback.
- **Returns:** The current offset as an `ArenaMarker`.

#### `int arena_restore(Arena *arena, ArenaMarker marker)`
Rewinds the arena's offset back to `marker`. Any allocations made after the marker was saved are discarded.
- **Parameters:**
  - `arena`: Target arena.
  - `marker`: Checkpoint previously returned by `arena_save`.
- **Returns:** `0` on success, `1` if `marker > arena->offset` (cannot rollback forward).

---

### 5. Scoped Scratchpad (TempArena)

The `TempArena` pattern pairs an arena with a marker, preventing human error from applying markers to the wrong arena instance.

#### `TempArena arena_temp_begin(Arena *arena)`
Opens a temporary memory scope by saving the current offset.
- **Returns:** A `TempArena` containing the arena pointer and the saved marker.

#### `int arena_temp_end(TempArena temp)`
Closes the temporary scope and automatically restores the associated arena back to its initial marker.
- **Returns:** `0` on success, `1` on error.

```c
void compute(Arena *arena) {
    TempArena scratch = arena_temp_begin(arena);

    // Perform intermediate calculations...
    float *matrix = arena_alloc_buffer(arena, float, 1024);
    // ...

    // Instant rollback of all scratch allocations
    arena_temp_end(scratch);
}
```

---

### 6. Duplication Utilities

#### `char *arena_dup_str(Arena *arena, const char *str)`
Duplicates a null-terminated C string into the arena, allocating `strlen(str) + 1` bytes and copying the terminating `\0`.
- **Returns:** Pointer to the duplicated string in the arena, or `NULL` if `str == NULL` or capacity was exceeded.

#### `void *arena_dup_data(Arena *arena, const void *data, size_t size)`
Allocates `size` bytes and copies the contents of `data` into the arena.
- **Returns:** Pointer to the newly allocated copy, or `NULL` on error.

---

### 7. Telemetry & Inspection

- **`size_t arena_used_bytes(const Arena *arena)`:** Returns the number of bytes currently consumed (including alignment padding).
- **`size_t arena_free_bytes(const Arena *arena)`:** Returns remaining available capacity (`capacity - offset`).
- **`size_t arena_capacity(const Arena *arena)`:** Returns total capacity of the backing buffer.

---

## Design Details & Guarantees

1. **Absolute Memory Alignment:** `arena_alloc_aligned` computes padding using the absolute numerical pointer address (`uintptr_t`), rather than only the relative offset. Even if the backing buffer begins at an odd address, requested alignments (e.g. 64-byte for AVX-512) are mathematically guaranteed.
2. **Overflow Safety:** All boundary computations calculate remaining space using subtraction (`size > capacity - start`) rather than addition (`start + size > capacity`), avoiding unsigned integer wraparound vulnerabilities.
3. **Array Allocation Overflow Guard:** `arena_alloc_buffer` verifies that `size * sizeof(type)` cannot exceed `SIZE_MAX` before attempting allocation.
4. **Ownership Invariant:** The `can_free` flag guarantees that arenas backed by stack memory or static storage will not trigger invalid `free()` calls when `arena_free()` is executed.
