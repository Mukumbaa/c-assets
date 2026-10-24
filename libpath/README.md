# libpath

A lightweight, robust, zero-dependency C library for cross-platform file path manipulation, normalization, and filesystem resolution.

`libpath` treats filesystem paths with the semantic rigor they require, solving common C pitfalls: slash inconsistencies across Unix and Windows, buffer overflows, path traversal vulnerabilities, and memory ownership ambiguity.

---

## Table of Contents

- [Key Architecture Principles](#key-architecture-principles)
- [Memory Model & Ownership](#memory-model--ownership)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
  - [1. Decomposition & Inspection (View-based)](#1-decomposition--inspection-view-based)
    - [`path_basename`](#const-char-path_basenameconst-char-path)
    - [`path_extension`](#const-char-path_extensionconst-char-path)
  - [2. Substring & Truncation (Heap-allocated)](#2-substring--truncation-heap-allocated)
    - [`path_dirname`](#char-path_dirnameconst-char-path)
    - [`path_stem`](#char-path_stemconst-char-path)
  - [3. Composition & Normalization](#3-composition--normalization)
    - [`path_set_extension`](#char-path_set_extensionconst-char-path-const-char-ext)
    - [`path_join`](#char-path_joinconst-char-base-const-char-sub)
    - [`path_normalize`](#char-path_normalizeconst-char-path)
  - [4. Relational & Security Queries](#4-relational--security-queries)
    - [`path_is_descendant_of`](#int-path_is_descendant_ofconst-char-path-const-char-base)
    - [`path_relative_to`](#char-path_relative_toconst-char-from-const-char-to)
  - [5. System & Filesystem Resolution](#5-system--filesystem-resolution)
    - [`path_to_absolute`](#char-path_to_absoluteconst-char-path)
    - [`path_canonicalize`](#char-path_canonicalizeconst-char-path)
- [Compilation & Running Tests](#compilation--running-tests)

---

## Key Architecture Principles

1. **Lexical (In-Memory) vs. Physical (Filesystem):**
   - Functions like `path_normalize`, `path_join`, and `path_relative_to` operate strictly in memory on character strings. They are deterministic, fast ($O(N)$), and do not require filesystem I/O.
   - Functions like `path_to_absolute` and `path_canonicalize` query the host operating system kernel to retrieve the working directory or resolve real physical links on disk.
2. **Cross-Platform Compatibility:**
   - Seamlessly handles both POSIX forward slashes (`/`) and Windows backslashes (`\`).
   - Understands Windows drive roots (e.g. `C:\`, `d:/`) and UNC network prefixes (`\\server\share`).
   - Automatically preserves or selects appropriate platform delimiters without corrupting paths.
3. **Safety & Security First:**
   - Immune to the "similar name prefix" vulnerability in sandboxing (`/var/log_secret` is not inside `/var/log`).
   - Immune to buffer underflows and string literal write-violations (`const char*` parameters).

---

## Memory Model & Ownership

`libpath` follows a strict and predictable memory contract:

| Category | Functions | Return Type | Memory Contract |
| :--- | :--- | :--- | :--- |
| **Views** | `path_basename`, `path_extension` | `const char *` | **Do NOT free.** Returns a pointer directly into the input buffer (or a static literal `""` / `"/""`). Zero allocations. |
| **Owned Buffers** | `path_dirname`, `path_stem`, `path_set_extension`, `path_join`, `path_normalize`, `path_relative_to`, `path_to_absolute`, `path_canonicalize` | `char *` | **MUST free.** Returns a newly allocated null-terminated string (`malloc`). Caller is responsible for calling `free()`. |
| **Booleans** | `path_is_descendant_of` | `int` | Returns `1` (`true`) or `0` (`false`). Cleans up all internal allocations prior to exit. |

---

## Quick Start

```c
#include <stdio.h>
#include <stdlib.h>
#include "libpath.h"

int main(void) {
    const char *raw = "/var/log/nginx/../app/./access.log";

    // 1. Inspect without allocations
    const char *name = path_basename(raw);   // "access.log"
    const char *ext  = path_extension(raw);  // "log"
    printf("File: %s (Extension: %s)\n", name, ext);

    // 2. Normalize redundant separators and parent navigation
    char *clean = path_normalize(raw);       // "/var/log/app/access.log"
    printf("Normalized: %s\n", clean);

    // 3. Switch extension
    char *backup = path_set_extension(clean, "bak"); // "/var/log/app/access.bak"
    printf("Backup path: %s\n", backup);

    // Free heap memory for owned buffers
    free(clean);
    free(backup);
    return 0;
}
```

---

## API Reference

### 1. Decomposition & Inspection (View-based)

#### `const char *path_basename(const char *path);`
Extracts the final component of a path (the filename or deepest folder name) without allocating memory.
- **Behavior:**
  - Ignores trailing directory separators (`"cartella/sub/"` $\rightarrow$ `"sub/"`).
  - Correctly isolates the root (`"/"` $\rightarrow$ `"/"`, `"///"` $\rightarrow$ `"/"`).
  - If no separators are present, returns the original input string.
  - If `path` is `NULL` or `""`, returns `""`.
- **Memory:** Zero allocation. Returns a slice inside `path` or a static literal. **Do not free.**

```c
path_basename("/usr/local/bin/gcc"); // Returns pointer to "gcc"
path_basename("file.txt");           // Returns pointer to "file.txt"
path_basename("/");                  // Returns "/"
```

---

#### `const char *path_extension(const char *path);`
Extracts the file extension **without the leading dot**.
- **Behavior:**
  - Operates strictly on the basename to avoid being fooled by dots in directory names (`"v1.0.4/binary"` $\rightarrow$ `""`).
  - Unix dotfiles without another extension are recognized as hidden files, not extensions (`".gitignore"` $\rightarrow$ `""`).
  - Files ending with a trailing dot return `""` (`"image."` $\rightarrow$ `""`).
  - Returns `""` if no extension exists, or if `path` is `NULL`/`""`.
- **Memory:** Zero allocation. Returns a slice inside `path` or a static literal. **Do not free.**

```c
path_extension("main.c");             // Returns "c"
path_extension("archive.tar.gz");     // Returns "gz"
path_extension(".gitignore");         // Returns ""
path_extension(".config.json");       // Returns "json"
path_extension("v1.0/binary");        // Returns ""
```

---

### 2. Substring & Truncation (Heap-allocated)

#### `char *path_dirname(const char *path);`
Computes the parent directory containing the target item.
- **Behavior:**
  - Returns `"."` if no directory separators exist (indicating the current working directory).
  - Preserves system roots (`"/file.txt"` $\rightarrow$ `"/"`, `"///file.txt"` $\rightarrow$ `"/"`).
  - Handles directories with trailing slashes (`"dir/sub/"` $\rightarrow$ `"dir"`).
  - Collapses duplicate slashes preceding the parent boundary.
  - If `path` is `NULL` or `""`, returns `"."`.
- **Memory:** Heap allocated. **Caller must free.**

```c
char *d1 = path_dirname("/usr/local/bin/gcc"); // "/usr/local/bin"
char *d2 = path_dirname("file.txt");          // "."
char *d3 = path_dirname("/file.txt");         // "/"
char *d4 = path_dirname("dir/sub/");          // "dir"
// free(d1); free(d2); free(d3); free(d4);
```

---

#### `char *path_stem(const char *path);`
Extracts the basename **excluding its file extension**.
- **Behavior:**
  - Strips only the final extension (`"archive.tar.gz"` $\rightarrow$ `"archive.tar"`).
  - Preserves entire name for files without extension (`"Makefile"` $\rightarrow$ `"Makefile"`).
  - Preserves hidden dotfile names (`".gitignore"` $\rightarrow$ `".gitignore"`, `".config.json"` $\rightarrow$ `".config"`).
  - System roots have no filename stem (`"/"` $\rightarrow$ `""`).
  - If `path` is `NULL` or `""`, returns an empty allocated string `""`.
- **Memory:** Heap allocated. **Caller must free.**

```c
char *s1 = path_stem("document.txt");     // "document"
char *s2 = path_stem("archive.tar.gz");   // "archive.tar"
char *s3 = path_stem(".gitignore");       // ".gitignore"
// free(s1); free(s2); free(s3);
```

---

### 3. Composition & Normalization

#### `char *path_set_extension(const char *path, const char *ext);`
Replaces, adds, or removes the extension of a path.
- **Behavior:**
  - **With or without dot:** Automatically formats the separator whether `ext` is `"md"` or `".md"`.
  - **Removal:** If `ext` is `""` or `NULL`, the existing extension and dot are stripped.
  - **Immune to directory dots:** Safe against parent directories containing periods (`"my.dir/file"` + `"o"` $\rightarrow$ `"my.dir/file.o"`).
  - **Dotfile safe:** Appending to a hidden file adds to the end (`".gitignore"` + `"txt"` $\rightarrow$ `".gitignore.txt"`).
- **Memory:** Heap allocated. **Caller must free.**

```c
char *p1 = path_set_extension("file.txt", "md");       // "file.md"
char *p2 = path_set_extension("file.txt", ".md");      // "file.md"
char *p3 = path_set_extension("Makefile", "bak");      // "Makefile.bak"
char *p4 = path_set_extension("image.png", NULL);      // "image"
// free(p1); free(p2); free(p3); free(p4);
```

---

#### `char *path_join(const char *base, const char *sub);`
Combines two path segments, inserting a delimiter only where necessary and adhering to standard overriding rules.
- **Behavior:**
  - **Absolute Override Rule:** If `sub` is an absolute path (`/`, `\`, `C:\`, `c:/`, `\\server`), it **overrides and discards `base`** entirely.
  - **Boundary Normalization:** Ensures exactly one separator exists at the boundary. Strips redundant duplicate trailing separators on `base`.
  - **Separator Selection:** Automatically respects Windows backslashes if present in `base`, otherwise defaults to `/`.
  - If `base` is `NULL`/`""`, returns a copy of `sub` (and vice-versa).
- **Memory:** Heap allocated. **Caller must free.**

```c
char *j1 = path_join("cartella", "file.txt");          // "cartella/file.txt"
char *j2 = path_join("cartella///", "file.txt");       // "cartella/file.txt"
char *j3 = path_join("/home/user", "/etc/hosts");      // "/etc/hosts" (Absolute override)
char *j4 = path_join("dir", "C:\\Windows\\cmd.exe");   // "C:\Windows\cmd.exe"
// free(j1); free(j2); free(j3); free(j4);
```

---

#### `char *path_normalize(const char *path);`
Pure in-memory lexical cleaner. Resolves `.` and `..` components algebraically without accessing the disk.
- **Behavior:**
  - Eliminates redundant single dots (`"a/./b"` $\rightarrow$ `"a/b"`).
  - Resolves double dots against preceding segments (`"a/b/../c"` $\rightarrow$ `"a/c"`).
  - **Root Ceiling Protection:** Double dots cannot climb above system roots (`"/.."` $\rightarrow$ `"/"`, `"/../../../etc"` $\rightarrow$ `"/etc"`).
  - **Relative Accumulation:** Retains leading double dots in relative paths (`"../../a"` $\rightarrow$ `"../../a"`).
  - Collapses duplicate slashes (`"a///b"` $\rightarrow$ `"a/b"`).
  - If a relative path cancels out completely, returns `"."` (`"a/b/../.."` $\rightarrow$ `"."`).
  - Empty string `""` normalizes to `"."`. `NULL` returns `""`.
- **Performance:** Implemented with a single-allocation token cursor. Zero auxiliary string splits.
- **Memory:** Heap allocated. **Caller must free.**

```c
char *n1 = path_normalize("a///b////c");               // "a/b/c"
char *n2 = path_normalize("/var/log/../run/./app");    // "/var/run/app"
char *n3 = path_normalize("/../../..");                // "/"
char *n4 = path_normalize("a/b/../..");                // "."
// free(n1); free(n2); free(n3); free(n4);
```

---

### 4. Relational & Security Queries

#### `int path_is_descendant_of(const char *path, const char *base);`
Determines whether `path` is contained within the directory hierarchy of `base`.
- **Security / Sandboxing:**
  - Pre-normalizes both inputs internally to prevent directory traversal escapes (`"uploads/../../etc/passwd"` vs `"uploads"` returns `0`).
  - **Prefix Boundary Check:** Avoids false-positive sibling matching (`"/var/log_secret"` is not a child of `"/var/log"`).
  - Rejects comparisons between mixed absolute and relative reference frames.
  - Considers a directory a descendant of itself (inclusive matching).
- **Returns:** `1` if `path` is inside `base`, `0` otherwise or on error/NULL.
- **Memory:** Automatically cleans up internal normalized strings. No caller cleanup required.

```c
path_is_descendant_of("/var/log/syslog", "/var/log");             // 1 (True)
path_is_descendant_of("/var/log_private/file", "/var/log");       // 0 (False - Sibling)
path_is_descendant_of("/var/www/sub/../../etc/passwd", "/var/www");// 0 (False - Traversal attack)
path_is_descendant_of("sub/file.txt", "/home/user");              // 0 (False - Mixed frames)
```

---

#### `char *path_relative_to(const char *from, const char *to);`
Calculates the relative path navigation steps needed to travel from `from` to `to`.
- **Behavior:**
  - Identifies the common ancestor fork directory between both paths.
  - Emits `..` for every unshared directory remaining in `from`.
  - Appends unshared directories remaining in `to`.
  - Identical paths return `"."`.
  - **Incompatibility Guards:** Returns `NULL` if mixing absolute and relative paths, or if Windows drive letters differ (`"C:\a"` vs `"D:\b"`).
- **Memory:** Heap allocated. **Caller must free.**

```c
char *r1 = path_relative_to("/a/b", "/a/b/c/d");       // "c/d"       (Pure descent)
char *r2 = path_relative_to("/a/b/c/d", "/a/b");       // "../.."     (Pure ascent)
char *r3 = path_relative_to("/a/b/c", "/a/x/y");       // "../../x/y" (Ascent + descent)
char *r4 = path_relative_to("/a/b", "/a/b");           // "."         (Identical)
char *r5 = path_relative_to("C:/a", "D:/b");           // NULL        (Different drives)
// Remember to free non-null results!
```

---

### 5. System & Filesystem Resolution

#### `char *path_to_absolute(const char *path);`
Converts a relative path to an absolute path by anchoring it to the process Current Working Directory (CWD), then normalizing.
- **Key Difference:** **Does NOT require the target path to exist on disk.** Ideal for constructing destination paths for files you intend to create.
- **Behavior:**
  - If `path` is already absolute, it simply normalizes and returns a copy.
  - If `path` is relative, queries CWD (`getcwd` on POSIX, `_getcwd` on Windows), joins, and normalizes.
  - Returns `NULL` if `path` is `NULL`. Empty string `""` resolves to the normalized CWD.
- **Memory:** Heap allocated. **Caller must free.**

```c
char *abs1 = path_to_absolute("src/main.c");       // "/home/user/project/src/main.c"
char *abs2 = path_to_absolute("../doc/manual.md"); // "/home/user/doc/manual.md"
char *abs3 = path_to_absolute("/usr/bin/gcc");     // "/usr/bin/gcc"
// free(abs1); free(abs2); free(abs3);
```

---

#### `char *path_canonicalize(const char *path);`
Resolves the true physical path on the storage hardware.
- **Key Difference:** **The file/folder MUST exist on the disk.**
- **Behavior:**
  - Recursively resolves all symbolic links (symlinks on Unix, junctions/shortcuts on Windows).
  - Resolves `.` and `..` based on physical disk layout rather than lexical syntax.
  - Fails and returns `NULL` if any component in the path does not exist on disk.
  - Uses native kernel syscalls: `realpath` on POSIX, `_fullpath` on Windows.
- **Memory:** Heap allocated. **Caller must free.**

```c
char *real1 = path_canonicalize(".");                     // Canonical absolute CWD
char *real2 = path_canonicalize("/var/mail");             // Resolves symlink -> "/var/spool/mail"
char *real3 = path_canonicalize("non_existent_file.xyz"); // NULL (File does not exist!)
// free(real1); free(real2);
```

