# libmap

A fast, lightweight, and memory-safe **Hash Map** library in pure C99 with string keys and generic `void*` values.

`libmap` is built using **Open Addressing with Linear Probing**, uses the **FNV-1a** hashing algorithm, and handles deletions via a **Tombstone recycling strategy**. It resizes automatically through dynamic **Rehashing** and guarantees zero memory leaks when properly destroyed.

---

## Table of Contents

- [Features](#features)
- [Data Structures & Types](#data-structures--types)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Initialization & Destruction](#initialization--destruction)
  - [Core Operations (Set, Get, Has, Remove)](#core-operations)
  - [Iteration (`next` & `foreach`)](#iteration)
  - [Utility & Hashing](#utility--hashing)
- [Design Details & Best Practices](#design-details--best-practices)
  - [Memory Ownership](#memory-ownership)
  - [Storing Primitive Types](#storing-primitive-types)
  - [Handling NULL Values](#handling-null-values)
  - [When to Use `next` vs `foreach`](#when-to-use-next-vs-foreach)
- [Compilation & Memory Verification](#compilation--memory-verification)

---

## Features

- **Open Addressing & Linear Probing:** High CPU cache locality with a single contiguous allocation for buckets.
- **Tombstone Recycling:** Slots marked as deleted (`(char *)-1`) are reused during subsequent insertions, keeping table probes short.
- **Dynamic Rehashing:** Automatically doubles capacity when reaching the load factor threshold ($\ge 75\%$), defragmenting tombstones and moving existing keys with zero redundant heap allocations.
- **FNV-1a Hashing:** Fast, uniform, non-cryptographic 32-bit (and 64-bit) hash functions.
- **Safe Memory Ownership:** Duplicates string keys internally with `strdup`, while leaving value pointers under user control.
- **Dual Iteration Models:** 
  - **Cursor-based (`hashmap_next`):** Imperative, pull-based loop with early-exit (`break`) support.
  - **Callback-based (`hashmap_foreach`):** Functional, push-based iteration passing a contextual `user_data` pointer.

---

## Data Structures & Types

Defined in `libmap.h`:

```c
typedef struct {
    char *key;       // NULL = empty slot, (char *)-1 = tombstone
    void *value;     // Pointer to the user-stored payload
    uint32_t hash;   // Precomputed 32-bit FNV-1a hash
} hashmap_entry;

typedef struct {
    hashmap_entry *he; // Dynamic array of entries
    int count;          // Number of active key-value pairs
    int capacity;       // Total allocated slots (power of 2, default: 32)
    int tombstone;      // Number of deleted slots
} hashmap;

// Callback signature for functional iteration
typedef void (*hashmap_callback)(const char *key, void *value, void *user_data);
```

---

## Quick Start

```c
#include <stdio.h>
#include <stdint.h>
#include "libmap.h"

// Callback for hashmap_foreach
static void print_entry(const char *key, void *value, void *user_data) {
    int *counter = (int *)user_data;
    (*counter)++;
    printf("[%d] %s => %s\n", *counter, key, (char *)value);
}

int main(void) {
    hashmap hm;
    hashmap_init(&hm);

    // Insert key-value pairs (0 = success)
    hashmap_set(&hm, "username", "alice");
    hashmap_set(&hm, "role", "admin");

    // Store an integer by casting through intptr_t
    hashmap_set(&hm, "port", (void *)(intptr_t)8080);

    // Retrieve values
    char *user = (char *)hashmap_get(&hm, "username");
    intptr_t port = (intptr_t)hashmap_get(&hm, "port");
    printf("User: %s, Port: %ld\n\n", user, (long)port);

    // 1. Functional iteration with hashmap_foreach
    printf("--- Iterating with hashmap_foreach ---\n");
    int count = 0;
    hashmap_foreach(&hm, print_entry, &count);

    // 2. Cursor iteration with hashmap_next
    printf("\n--- Iterating with hashmap_next ---\n");
    int iter = 0;
    char *k = NULL;
    void *v = NULL;
    while (hashmap_next(&hm, &iter, &k, &v)) {
        printf("Key: %s\n", k);
    }

    // Free all resources
    hashmap_destroy(&hm);
    return 0;
}
```

---

## API Reference

### Initialization & Destruction

#### `void hashmap_init(hashmap *hm)`
Initializes the hash map with a default capacity of 32 slots and zeroes out all counters.
- **Parameters:** `hm` – Pointer to a `hashmap` struct.

#### `void hashmap_destroy(hashmap *hm)`
Frees all heap-allocated string keys and the underlying entry array `he`. Sets `hm->he = NULL` and resets all fields to 0 to prevent dangling pointers.
- **Parameters:** `hm` – Pointer to the map. Safe to call multiple times or with a `NULL` pointer.

---

### Core Operations

#### `int hashmap_set(hashmap *hm, const char *key, void *value)`
Inserts a new key-value pair or updates the value if the key already exists.
- Automatically triggers `hashmap_rehashing` when `count >= capacity * 0.75`.
- Copies the key using `strdup`. If the key already exists, updates `value` in place without reallocating the key string.
- Recycles the first encountered tombstone if the key is new.
- **Parameters:**
  - `hm` – Pointer to the map.
  - `key` – Null-terminated string key.
  - `value` – Pointer or payload to store.
- **Returns:** `0` on success, `1` on error (`NULL` parameters or allocation failure).

#### `void *hashmap_get(hashmap *hm, const char *key)`
Searches for `key` and returns the associated pointer value.
- Performs a fast integer comparison (`hash == hash`) before calling `strcmp`.
- Traverses through tombstones without stopping; terminates immediately on encountering an empty slot (`NULL`).
- **Parameters:** `hm` – Pointer to the map, `key` – Key to look up.
- **Returns:** Stored `void *` on match, or `NULL` if not found (or on invalid input).

#### `int hashmap_has(hashmap *hm, const char *key)`
Checks whether a key exists in the map. Useful when stored values might legitimately be `NULL`.
- **Parameters:** `hm` – Pointer to the map, `key` – Key to check.
- **Returns:** `1` if key exists, `0` if not found or on invalid input.

#### `int hashmap_remove(hashmap *hm, const char *key)`
Removes `key` from the map, deallocating the internal string key with `free` and replacing the slot with a tombstone (`(char *)-1`).
- Decrements `hm->count` and increments `hm->tombstone`.
- **Parameters:** `hm` – Pointer to the map, `key` – Key to remove.
- **Returns:** `0` on successful removal, `1` if the key was not found or input was invalid.

---

### Iteration

`libmap` provides two complementary ways to traverse all active elements:

#### `int hashmap_next(hashmap *hm, int *iter, char **key, void **value)`
**Cursor-based iterator (Pull Model).** Yields active key-value pairs one by one, skipping empty and tombstone slots.
- **Usage:**
  - Initialize an integer cursor variable to `0` (e.g., `int iter = 0;`).
  - Pass its address `&iter` on each call. The function updates `*iter` to point to the next search position.
  - Returns `1` each time an active entry is found, and `0` when the end of the table is reached.
- **Parameters:**
  - `hm` – Pointer to the map.
  - `iter` – Pointer to the cursor state variable (must start at 0).
  - `key` – Address of a `char *` variable to store the yielded key pointer.
  - `value` – Address of a `void *` variable to store the yielded value pointer.
- **Returns:** `1` if an element was yielded, `0` when iteration finishes.

#### `void hashmap_foreach(const hashmap *hm, hashmap_callback cb, void *user_data)`
**Callback-based iterator (Push Model).** Iterates through the entire map and invokes the callback function for every active entry.
- Skips empty slots and tombstones automatically.
- Safely does nothing if `hm` or `cb` is `NULL`.
- The `user_data` pointer allows the callback to access caller-defined local state (simulating a closure).
- **Parameters:**
  - `hm` – Pointer to the map.
  - `cb` – Callback function of type `hashmap_callback`.
  - `user_data` – Arbitrary pointer passed directly into the callback on each invocation (can be `NULL`).

---

### Utility & Hashing

#### `uint32_t fnv1a_32(const char *key)`
Computes the 32-bit Fowler–Noll–Vo (FNV-1a) hash for the given string.

#### `uint64_t fnv1a_64(const char *key)`
Computes the 64-bit Fowler–Noll–Vo (FNV-1a) hash for the given string.

#### `size_t get_index(uint32_t hash, int capacity)`
Computes the slot index using a bitwise mask: `hash & (capacity - 1)`. Assumes `capacity` is a power of 2.

---

## Design Details & Best Practices

### Memory Ownership
- **Keys:** `libmap` takes **full ownership** of keys. Every new key is duplicated via `strdup` during insertion, and automatically freed with `free` upon removal (`hashmap_remove`) or destruction (`hashmap_destroy`).
- **Values:** `libmap` stores raw `void*` pointers and **does not manage their lifecycle**. If your values point to heap memory, free them before calling `hashmap_destroy`.

### Storing Primitive Types
Because pointers (`void *`) on modern 32-bit and 64-bit systems are at least as large as integers, you can store `int`, `size_t`, `bool`, or enum values directly without heap allocations using `intptr_t` casts:

```c
// Storing an int:
int id = 42;
hashmap_set(&hm, "user_id", (void *)(intptr_t)id);

// Reading an int:
int retrieved_id = (int)(intptr_t)hashmap_get(&hm, "user_id");
```

### Handling NULL Values
If you insert a `NULL` value:
```c
hashmap_set(&hm, "optional_setting", NULL);
```
Calling `hashmap_get(&hm, "optional_setting")` will return `NULL`. To distinguish whether the key does not exist or exists with a `NULL` value, use `hashmap_has`:
```c
if (hashmap_has(&hm, "optional_setting")) {
    // Key exists!
}
```

### When to Use `next` vs `foreach`

| Use Case | Recommended Approach | Reason |
| :--- | :--- | :--- |
| **Search / Early Exit** | `hashmap_next` | You can stop the `while` loop anytime using `break`. |
| **Simple Logging / Printing** | `hashmap_foreach` | Clean one-liner, no loop boilerplate. |
| **Accumulating Metrics / Sums** | `hashmap_foreach` | Pass accumulator struct via `user_data`. |
| **Nested Iteration / State Machines** | `hashmap_next` | Each loop maintains its own separate cursor variable. |

---

## Compilation & Memory Verification

### Standard Compilation
Compile `libmap.c` together with your source files:
```bash
gcc -Wall -Wextra -g libmap.c main.c -o my_app
```

### AddressSanitizer (ASan)
Check for memory bugs, out-of-bounds accesses, and leaks directly during execution:
```bash
gcc -Wall -Wextra -g -fsanitize=address,undefined libmap.c main.c -o my_app
./my_app
```

### Valgrind
Verify that 100% of heap memory is freed with zero leaks:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./my_app
```
Expected output:
```text
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```
