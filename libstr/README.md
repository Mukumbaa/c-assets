# libstr

A lightweight, robust, and safe dynamic string library written in standard C99.

`libstr` provides an ergonomic, growable string type (`String`) that eliminates common pitfalls associated with raw C-strings (buffer overflows, off-by-one errors, memory leaks, and use-after-free conditions during reallocations). It includes built-in protection against self-aliasing, integer overflows on 32/64-bit systems, and leak-free error handling.

---

## Table of Contents

- [Features](#features)
- [Data Structures & Constants](#data-structures--constants)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [Initialization & Destruction](#1-initialization--destruction)
  - [Buffer & Capacity Management](#2-buffer--capacity-management)
  - [Appending & Concatenation](#3-appending--concatenation)
  - [Extraction, Removal & Replacement](#4-extraction-removal--replacement)
  - [Trimming & In-Place Transformations](#5-trimming--in-place-transformations)
  - [Comparison & Search Queries](#6-comparison--search-queries)
  - [Tokenization & Joining](#7-tokenization--joining)
- [Memory Management & Safety Guarantees](#memory-management--safety-guarantees)

---

## Features

- **Geometric Growth Policy:** Automatically expands buffers using a $1.5\times$ growth factor to maintain amortized $O(1)$ appends.
- **Aliasing Safety:** Functions like `str_cat` and `str_append` safely handle appending a string to itself (`str_append(&s, &s)`), preventing use-after-free bugs when `realloc` moves memory.
- **Integer Overflow Protection:** Extensive checks against `SIZE_MAX` prevent arithmetic wrapping attacks during allocation calculations.
- **Zero Memory Leaks on Errors:** Functions like `str_split` roll back and clean up all allocated tokens if a reallocation fails mid-process.
- **Hardware-Optimized Operations:** Built on top of `<string.h>` (`memcpy`, `memmove`, `memcmp`) for SIMD-accelerated throughput.

---

## Data Structures & Constants

### `String`
```c
typedef struct {
    char *str;        // Pointer to the null-terminated heap buffer
    size_t len;       // Number of active characters (excluding '\0')
    size_t capacity;  // Total allocated memory size in bytes
} String;
```

### Search Mode Flags
Used by search routines (`str_contains`, `str_contains_cstr`, `str_find_last`):
- `CASE_SENSITIVE (1)`: Performs strict byte-for-byte character comparison.
- `NO_CASE_SENSITIVE (0)`: Compares characters case-insensitively using `tolower()`.

---

## Quick Start

```c
#include <stdio.h>
#include "libstr.h"

int main(void) {
    // Initialize from a C-string
    String greeting = str_init("Hello");

    // Append characters and formatted strings
    str_push_char(&greeting, ',');
    str_append_cstr(&greeting, " world");
    str_append_fmt(&greeting, "! Total count: %d", 42);

    printf("%s\n", greeting.str); // "Hello, world! Total count: 42"

    // Always free heap memory when done
    str_destroy(&greeting);
    return 0;
}
```

---

## API Reference

### 1. Initialization & Destruction

#### `String str_init(const char *s)`
Creates a dynamic string by copying an existing null-terminated C-string. Allocates at least 16 bytes of initial capacity to prevent immediate reallocations on subsequent appends.
- **Parameters:** `s` – Source null-terminated string (passing `NULL` returns `str_empty()`).
- **Returns:** A newly initialized `String`.

#### `String str_init_with_cap(size_t initial_capacity)`
Allocates an empty string with a specific preallocated buffer size.
- **Parameters:** `initial_capacity` – Minimum capacity to allocate (clamped to at least 16 bytes).
- **Returns:** A `String` with `len = 0` and `capacity >= initial_capacity`.

#### `String str_empty(void)`
Creates a zero-length string with the default initial capacity (16 bytes) containing `""`.
- **Returns:** An empty initialized `String`.

#### `String str_from_fmt(const char *fmt, ...)`
Constructs a string directly using standard `printf`-style formatting specifiers.
- **Parameters:** `fmt` – Printf format string followed by variadic arguments.
- **Returns:** A new `String` with the formatted content. Returns `str_empty()` if formatting fails.

#### `String str_dup(const String *src)`
Creates a deep copy of an existing `String`, allocating a new buffer with exact sizing.
- **Parameters:** `src` – Pointer to the string to duplicate.
- **Returns:** An exact duplicate `String`. Returns `str_empty()` if `src` is `NULL`.

#### `void str_destroy(String *str)`
Releases the underlying heap buffer and resets `str->str = NULL`, `len = 0`, and `capacity = 0`.
- **Parameters:** `str` – Pointer to the target string. Safe to call on already-destroyed or zero-initialized strings.

#### `void str_clear(String *str)`
Empties the string by placing a null-terminator at index `0` and setting `len = 0`. Does **not** free the heap buffer, making it optimal for reusing buffers in loops.
- **Parameters:** `str` – Pointer to the target string.

#### `void str_reset(String *str)`
Resets the string length to `0`. If the string was never allocated or had its capacity set to `0`, it allocates default storage.
- **Parameters:** `str` – Pointer to the target string.

---

### 2. Buffer & Capacity Management

#### `bool str_reserve(String *str, size_t min_capacity)`
Ensures that the internal buffer has at least `min_capacity` bytes available. If `capacity >= min_capacity`, this function is a no-op.
- **Parameters:** 
  - `str` – Pointer to the target string.
  - `min_capacity` – Desired minimum buffer capacity.
- **Returns:** `true` on success, `false` if `realloc` fails or `str` is `NULL`.

#### `bool str_shrink_to_fit(String *str)`
Releases unused heap memory by downsizing `capacity` to match exactly `len + 1`.
- **Parameters:** `str` – Pointer to the target string.
- **Returns:** `true` on success, `false` on allocation error.

---

### 3. Appending & Concatenation

#### `bool str_cat(String *dest, const String *src, size_t index)`
Inserts the contents of `src` into `dest` at the specified `index`. Handles buffer expansion and safely offsets subsequent characters using `memmove`.
- **Aliasing Safety:** Safely supports self-insertion (e.g. `str_cat(&s, &s, 0)`).
- **Parameters:**
  - `dest` – Destination string to modify.
  - `src` – Source string to insert.
  - `index` – Character offset where insertion begins (`0 <= index <= dest->len`).
- **Returns:** `true` on success, `false` on out-of-bounds `index`, allocation failure, or overflow.

#### `bool str_append(String *dest, const String *src)`
Appends the dynamic string `src` to the end of `dest`.
- **Aliasing Safety:** Safely supports self-appending (`str_append(&s, &s)`).
- **Parameters:** `dest`, `src` – Target and source strings.
- **Returns:** `true` on success, `false` on error.

#### `bool str_append_cstr(String *dest, const char *src)`
*(Alias: `str_catr`)*  
Appends a raw, null-terminated C-string to the end of `dest`.
- **Parameters:**
  - `dest` – Destination dynamic string.
  - `src` – Source raw C-string.
- **Returns:** `true` on success, `false` if inputs are `NULL` or memory allocation fails.

#### `bool str_append_fmt(String *dest, const char *fmt, ...)`
Appends formatted output to the end of `dest` using `printf`-style formatting without intermediate manual buffers.
- **Parameters:** `dest`, `fmt`, `...` – Destination and formatting arguments.
- **Returns:** `true` on success, `false` on formatting or allocation errors.

#### `bool str_push_char(String *dest, char c)`
*(Alias: `str_char`)*  
Appends a single character `c` to the end of the string, growing the buffer if needed.
- **Parameters:** `dest` – Target string, `c` – Character to push.
- **Returns:** `true` on success, `false` on failure.

#### `bool str_pop(String *dest, char *out_char)`
Removes the last character of the string and maintains valid null-termination.
- **Parameters:**
  - `dest` – Target string.
  - `out_char` – Optional pointer where the popped character will be stored (can be `NULL`).
- **Returns:** `true` if a character was removed, `false` if the string was empty or `NULL`.

#### `bool str_cpy(String *dest, const String *src)`
Overwrites the content of `dest` with a copy of `src`. Reallocates only if `dest->capacity` is insufficient.
- **Parameters:** `dest` – Destination string, `src` – Source string.
- **Returns:** `true` on success, `false` on allocation error.

---

### 4. Extraction, Removal & Replacement

#### `String str_substr(const String *s, size_t start, size_t count)`
Extracts a new sub-string starting at index `start` with a maximum length of `count` characters.
- **Parameters:**
  - `s` – Source string.
  - `start` – Zero-based starting index.
  - `count` – Maximum number of characters to copy.
- **Returns:** A newly allocated `String`. Returns `str_empty()` if parameters are out of range.

#### `bool str_remove(String *s, size_t index, size_t count)`
Deletes `count` characters in place starting from `index` and shifts subsequent characters left.
- **Parameters:**
  - `s` – Target string to modify.
  - `index` – Starting offset for removal.
  - `count` – Number of characters to delete.
- **Returns:** `true` on success, `false` if `index >= s->len` or `count == 0`.

#### `bool str_replace(String *s, const char *old_sub, const char *new_sub)`
Replaces all occurrences of `old_sub` with `new_sub` in-place.
- **Parameters:**
  - `s` – String to perform replacements on.
  - `old_sub` – Target substring to find.
  - `new_sub` – Substring to insert in place of `old_sub`.
- **Returns:** `true` on success, `false` if `old_sub` is empty or allocation fails.

---

### 5. Trimming & In-Place Transformations

#### `void str_ltrim(String *s)`
Strips leading whitespace (as determined by `isspace()`) in place by shifting remaining characters left.

#### `void str_rtrim(String *s)`
Strips trailing whitespace in place by placing a null-terminator at the first non-whitespace character from the end.

#### `void str_trim(String *s)`
Strips both leading and trailing whitespace characters in place.

#### `void str_to_lower(String *s)`
Converts all ASCII characters in the string to lowercase in place via `tolower()`.

#### `void str_to_upper(String *s)`
Converts all ASCII characters in the string to uppercase in place via `toupper()`.

#### `void str_reverse(String *s)`
Reverses the characters of the string in place using a two-pointer swap.

---

### 6. Comparison & Search Queries

#### `int str_cmp(const String *str1, const String *str2)`
Lexicographically compares two dynamic strings.
- **Returns:** `< 0` if `str1 < str2`, `0` if identical, `> 0` if `str1 > str2`. Handles `NULL` pointers safely.

#### `int str_cmp_cstr(const String *str1, const char *str2)`
*(Alias: `str_cmpr`)*  
Lexicographically compares a dynamic string with a raw C-string.
- **Returns:** `< 0`, `0`, or `> 0` matching `strcmp` semantics.

#### `bool str_equals(const String *str1, const String *str2)`
Performs an optimized equality check. Compares string lengths first, performing a memory comparison only if lengths match.
- **Returns:** `true` if both strings are non-null and identical in content, `false` otherwise.

#### `bool str_equals_cstr(const String *str1, const char *str2)`
*(Alias: `str_equalsr`)*  
Checks whether the dynamic string matches a raw null-terminated C-string.
- **Returns:** `true` if contents match, `false` otherwise.

#### `bool str_is_empty(const String *s)`
Checks if the string is empty.
- **Returns:** `true` if `s == NULL` or `s->len == 0`, `false` otherwise.

#### `bool str_starts_with(const String *s, const char *prefix)`
Tests if the string begins with the specified prefix.
- **Returns:** `true` if `s` starts with `prefix`, `false` otherwise.

#### `bool str_ends_with(const String *s, const char *suffix)`
Tests if the string ends with the specified suffix.
- **Returns:** `true` if `s` ends with `suffix`, `false` otherwise.

#### `ptrdiff_t str_contains(const String *str, const String *pattern, int is_case_sensitive)`
Searches for the first occurrence of `pattern` inside `str`.
- **Parameters:**
  - `str` – String to search within.
  - `pattern` – Dynamic string pattern to look for.
  - `is_case_sensitive` – `CASE_SENSITIVE (1)` or `NO_CASE_SENSITIVE (0)`.
- **Returns:** Zero-based index of the first match, or `-1` if not found.

#### `ptrdiff_t str_contains_cstr(const String *str, const char *pattern, int is_case_sensitive)`
*(Alias: `str_containsr`)*  
Searches for the first occurrence of a raw C-string `pattern` inside `str`.
- **Returns:** Zero-based index of the first match, or `-1` if not found.

#### `ptrdiff_t str_find_last(const String *str, const char *pattern, int is_case_sensitive)`
Scans backwards to find the last occurrence of a raw C-string `pattern` inside `str`.
- **Returns:** Zero-based index of the match, or `-1` if not found.

---

### 7. Tokenization & Joining

#### `int str_split(String **arr, const String *str, char delimiter)`
Splits a string by a single delimiter character into a newly allocated array of dynamic strings.
- **Memory Safety:** If any allocation fails halfway through, all previously allocated tokens and the array are freed automatically, leaving zero memory leaks.
- **Parameters:**
  - `arr` – Pointer to a `String*` where the resulting array will be assigned.
  - `str` – Input string to tokenize.
  - `delimiter` – Splitting character (e.g. `','` or `' '`).
- **Returns:** Total number of tokens created, or `-1` on failure.

```c
String *tokens = NULL;
String input = str_init("apple,banana,cherry");

int count = str_split(&tokens, &input, ',');
if (count > 0) {
    for (int i = 0; i < count; i++) {
        printf("[%d]: %s\n", i, tokens[i].str);
    }
    // Clean up all tokens and the container array
    str_free_split(tokens, count);
}

str_destroy(&input);
```

#### `bool str_join(String *dest, const String *arr, int count, const char *separator)`
Concatenates an array of `String` objects into `dest`, separated by an optional delimiter.
- **Optimization:** Scans token lengths beforehand to compute total required memory, resizing `dest` exactly once to avoid multiple reallocations.
- **Parameters:**
  - `dest` – Destination string (overwritten with joined result).
  - `arr` – Array of `String` elements.
  - `count` – Number of items in `arr`.
  - `separator` – Delimiter inserted between elements (can be `NULL` or `""`).
- **Returns:** `true` on success, `false` on allocation failure.

#### `void str_free_split(String *arr, int count)`
Frees an array of tokens previously created by `str_split`. Destroys each individual `String` element before freeing the array pointer itself.
- **Parameters:** `arr` – Token array pointer, `count` – Number of elements in `arr`.

---

## Memory Management & Safety Guarantees

1. **Destruction Responsibility:** Every `String` created via `str_init`, `str_init_with_cap`, `str_empty`, `str_from_fmt`, `str_dup`, or `str_substr` owns its heap buffer and must be released with `str_destroy(&s)`.
2. **Buffer Invariance:** A valid `String` will always have `s.str[s.len] == '\0'`. Even if dynamic resizing occurs, null-termination is consistently preserved.
3. **Aliasing Safe:** Operations such as `str_append(&s, &s)` or `str_cat(&s, &s, 2)` are checked for memory overlap. If `src` references memory within `dest`, an isolated copy is preserved during buffer relocation.
4. **Tokenization Cleanup:** Never use standard `free(tokens)` on arrays returned by `str_split`. Always call `str_free_split(tokens, count)` to ensure both the internal string buffers and the container array are freed.
