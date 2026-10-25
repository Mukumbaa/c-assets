# libmap

A fast, lightweight, and memory-safe **Hash Map** library in pure C99 with string keys and generic `void*` values.

`libmap` is packaged as a modern **single-header library**, built using **Open Addressing with Linear Probing**, uses the **FNV-1a** hashing algorithm, and handles deletions via a **Tombstone recycling strategy**. It resizes automatically through dynamic **Rehashing** and provides type-casting macros for clean, expressive C code.

---

## Table of Contents

- [Features](#features)
- [How to Include](#how-to-include)
- [Data Structures & Types](#data-structures--types)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Initialization & Destruction](#initialization--destruction)
  - [Core Operations](#core-operations)
  - [Type-Safe Retrieval & Helper Macros](#type-safe-retrieval--helper-macros)
  - [Iteration (`for_each`, `next`, `foreach`)](#iteration)
  - [Utility & Metrics](#utility--metrics)
- [Design Details & Best Practices](#design-details--best-practices)
  - [Memory Ownership](#memory-ownership)
  - [Storing Primitive Types](#storing-primitive-types)
  - [Handling NULL Values](#handling-null-values)
  - [Choosing the Right Iteration Model](#choosing-the-right-iteration-model)
- [Compilation & Memory Verification](#compilation--memory-verification)
- [License](#license)

---

## Features

- **Single-Header Library:** Zero external dependencies beyond the C standard library.
- **Open Addressing & Linear Probing:** High CPU cache locality with a single contiguous allocation for buckets.
- **Tombstone Recycling:** Slots marked as deleted (`(char *)-1`) are reused during subsequent insertions, keeping probe sequences short.
- **Dynamic Rehashing:** Automatically doubles capacity when reaching the load factor threshold ($\ge 75\%$), defragmenting tombstones and moving keys with zero redundant heap allocations.
- **FNV-1a Hashing:** Uniform, fast, non-cryptographic 32-bit hash function.
- **Safe Memory Ownership:** Duplicates string keys internally with `strdup`, while leaving value lifecycles under user control.
- **Ergonomic Helper Macros:** Cast `void*` payloads directly to concrete types (`hashmap_get_type`), provide default fallbacks (`hashmap_get_or_type`), and write natural `for` loops (`hashmap_for_each`).

---

## How to Include

`libmap` follows the single-header format. In **exactly one** `.c` file, define `LIBMAP_IMPLEMENTATION` before including `libmap.h`:

```c
#define LIBMAP_IMPLEMENTATION
#include "libmap.h"
```

In all other source files, simply include the header normally:

```c
#include "libmap.h"
```

---

## Data Structures & Types

Defined in `libmap.h`:

```c
typedef struct {
    char *key;       // NULL = empty slot, (char *)-1 = tombstone
    void *value;     // Pointer to user-stored payload
    uint32_t hash;   // Precomputed 32-bit FNV-1a hash
} hashmap_entry;

typedef struct {
    hashmap_entry *he; // Dynamic array of entry buckets
    size_t count;      // Number of active key-value pairs
    size_t capacity;   // Total allocated slots (power of 2, default: 32)
    size_t tombstone;  // Number of deleted tombstone slots
} hashmap;

// Callback signature for functional iteration
typedef void (*hashmap_callback)(const char *key, void *value, void *user_data);
```

---

## Quick Start

```c
#include <stdio.h>
#include <stdint.h>

#define LIBMAP_IMPLEMENTATION
#include "libmap.h"

typedef struct {
    int id;
    const char *role;
} User;

int main(void) {
    hashmap hm;
    hashmap_init(&hm);

    User alice = { .id = 1, .role = "admin" };
    User bob   = { .id = 2, .role = "editor" };
    User guest = { .id = 0, .role = "guest" };

    // 1. Insert pointer payloads (0 = success)
    hashmap_set(&hm, "alice", &alice);
    hashmap_set(&hm, "bob", &bob);

    // 2. Type-safe retrieval via macro (no manual casting needed)
    User *u = hashmap_get_type(&hm, "alice", User);
    printf("Alice's role: %s\n", u->role);

    // 3. Fallback retrieval if key does not exist
    User *missing = hashmap_get_or_type(&hm, "charlie", User, &guest);
    printf("Charlie (fallback): %s\n", missing->role);

    // 4. Natural loop iteration using hashmap_for_each
    printf("\n--- Active Users ---\n");
    const char *key;
    User *val;
    hashmap_for_each(&hm, it, key, val) {
        printf("Key: %-10s => ID: %d, Role: %s\n", key, val->id, val->role);
    }

    // 5. Cleanup
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

#### `void hashmap_init_wcapacity(hashmap *hm, size_t capacity)`
Initializes the map with a custom initial capacity. `capacity` must be a power of 2.
- **Parameters:**
  - `hm` – Pointer to the map.
  - `capacity` – Initial slot count.

#### `void hashmap_destroy(hashmap *hm)`
Frees all heap-allocated string keys and the underlying entry array `he`. Sets `hm->he = NULL` and resets all fields to 0 to prevent dangling pointers.
- **Parameters:** `hm` – Pointer to the map. Safe to call multiple times or with `NULL`.

---

### Core Operations

#### `int hashmap_set(hashmap *hm, const char *key, void *value)`
Inserts a new key-value pair or updates the value if the key already exists.
- Automatically triggers table expansion when `count >= capacity * 0.75`.
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

### Type-Safe Retrieval & Helper Macros

To eliminate repetitive manual casts `(MyType *)hashmap_get(...)`, `libmap` provides type-safe macros:

#### `hashmap_get_type(hm, key, type)`
Retrieves `key` and casts the resulting pointer directly to `(type *)`.
```c
Config *cfg = hashmap_get_type(&hm, "app_config", Config);
```

#### `static inline void *hashmap_get_or(hashmap *hm, const char *key, void *default_val)`
Looks up `key`. If found, returns its pointer; otherwise, returns `default_val` in a single probe without redundant hash recalculation.

#### `hashmap_get_or_type(hm, key, type, default_val)`
Type-safe wrapper over `hashmap_get_or`:
```c
int default_port = 8080;
int *port = hashmap_get_or_type(&hm, "port", int, &default_port);
```

#### `hashmap_count(hm)`
Evaluates to the number of active entries (`hm->count`).

#### `hashmap_is_empty(hm)`
Evaluates to `1` if the map contains zero active elements, `0` otherwise.

---

### Iteration

`libmap` provides three complementary ways to traverse active elements:

#### 1. Idiomatic `for` Loop: `hashmap_for_each`
A clean macro wrapper around `hashmap_next`. Supports standard control flow statements (`break`, `continue`, `return`):
```c
const char *key;
User *user;
hashmap_for_each(&hm, it, key, user) {
    if (user->id == 42) {
        printf("Found target: %s\n", key);
        break; // Early exit supported!
    }
}
```

#### 2. Cursor-Based Iterator: `hashmap_next`
Pull-based cursor iteration. Yields entries one by one, skipping empty and tombstone slots.
```c
int iter = 0;
char *k = NULL;
void *v = NULL;
while (hashmap_next(&hm, &iter, &k, &v)) {
    printf("%s => %p\n", k, v);
}
```

#### 3. Callback-Based Visitor: `hashmap_foreach`
Push-based functional visitor passing a contextual `user_data` pointer:
```c
void my_visitor(const char *key, void *value, void *user_data) {
    int *total = (int *)user_data;
    (*total)++;
}

int count = 0;
hashmap_foreach(&hm, my_visitor, &count);
```

---

### Utility & Metrics

#### `uint32_t fnv1a_32(const char *key)`
Computes the 32-bit Fowler–Noll–Vo (FNV-1a) hash for the given string.

#### `uint64_t fnv1a_64(const char *key)`
Computes the 64-bit Fowler–Noll–Vo (FNV-1a) hash for the given string.

---

## Design Details & Best Practices

### Memory Ownership
- **Keys:** `libmap` takes **full ownership** of keys. Every inserted key is duplicated via `strdup` and automatically freed with `free` upon removal (`hashmap_remove`) or destruction (`hashmap_destroy`).
- **Values:** `libmap` stores raw `void*` pointers and **does not manage their lifecycle**. If your values point to dynamically allocated heap memory, ensure you free them before calling `hashmap_destroy`.

### Storing Primitive Types
Because pointers (`void *`) on modern 32-bit and 64-bit architectures are at least as large as standard integer types, you can store integer values directly inside the pointer field without allocating heap memory by casting through `uintptr_t`:

```c
// Storing an int directly:
int id = 42;
hashmap_set(&hm, "user_id", (void *)(uintptr_t)id);

// Retrieving an int directly:
int retrieved_id = (int)(uintptr_t)hashmap_get(&hm, "user_id");
```

### Handling NULL Values
If you store a `NULL` value:
```c
hashmap_set(&hm, "optional_flag", NULL);
```
`hashmap_get(&hm, "optional_flag")` will return `NULL`. To distinguish between a key that does not exist and a key that exists with a `NULL` payload, use `hashmap_has`:
```c
if (hashmap_has(&hm, "optional_flag")) {
    // Key is present in the table!
}
```

### Choosing the Right Iteration Model

| Approach | Ideal Use Case | Highlights |
| :--- | :--- | :--- |
| **`hashmap_for_each`** | General-purpose loops | Most idiomatic; supports `break` and `continue`. |
| **`hashmap_next`** | Stateful / nested iterations | Manual cursor control across functions. |
| **`hashmap_foreach`** | Functional pipelines | Clean callback syntax; encapsulating operations. |

---

## Compilation & Memory Verification

### Single-Header Compilation
Include `libmap.h` in your project and compile with any C99 compiler:
```bash
gcc -Wall -Wextra -g main.c -o my_app
```

### Memory Sanitizers (ASan)
Verify safe memory execution and bounds checking:
```bash
gcc -Wall -Wextra -g -fsanitize=address,undefined main.c -o my_app
./my_app
```

### Valgrind Verification
Ensure zero memory leaks across insertions, deletions, and destructions:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./my_app
```
Expected output:
```text
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```
