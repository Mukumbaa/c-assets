# libini

A modern, robust, single-header schema-based INI configuration parser for C99.

Unlike traditional INI parsers that load raw text into generic dictionaries requiring manual key lookup and string-to-type conversions, `libini` operates on a **declarative schema-binding** model. You register target pointers, and the parser handles type validation, boundary checks, choice constraints, and struct population automatically.

---

## Table of Contents

- [Key Features](#key-features)
- [Design Philosophy: Schema Binding vs. Blind Lookup](#design-philosophy-schema-binding-vs-blind-lookup)
- [Quick Start](#quick-start)
- [INI Format & Syntax Rules](#ini-format--syntax-rules)
- [API Reference](#api-reference)
  - [Lifecycle Management](#1-lifecycle-management)
  - [Schema Binding Helpers](#2-schema-binding-helpers)
  - [Parsing Functions](#3-parsing-functions)
  - [Status Codes](#4-status-codes)
- [Comprehensive Example](#comprehensive-example)
  - [Configuration File (`app.ini`)](#configuration-file-appini)
  - [Implementation (`main.c`)](#implementation-mainc)
- [Error Handling & Strict Validation](#error-handling--strict-validation)
- [Integration with the C Toolkit](#integration-with-the-c-toolkit)
- [License](#license)

---

## Key Features

- **Single-Header (`libini.h`):** Zero external dependencies; requires only standard C library headers (`stdio.h`, `stdlib.h`, `string.h`, `stdbool.h`, `ctype.h`, `errno.h`).
- **Direct Struct Binding:** Automatically parses and converts INI values directly into target memory locations (`bool`, `int`, `double`, `char`, fixed-size `char[]`).
- **Fail-Fast Strict Validation:** Rejects typos and unexpected keys immediately with precise line numbers (e.g., stops execution on `prt = 8080` instead of `port = 8080`).
- **Mandatory Fields Enforcement:** Marks critical keys as `required`; reports missing mandatory options after parsing.
- **Allowed Choices Validation:** Enforces value sets natively (e.g., restricting an environment key to `"dev,staging,prod"`).
- **Safe String Copying:** Fixed-size string buffers require an explicit buffer size, completely preventing buffer overflows.
- **Quote & Inline Comment Aware:** Strips inline comments (`#`, `;`) while preserving comment characters inside quoted strings.
- **Case-Insensitive Matching:** Section and key lookups match case-insensitively, consistent with traditional INI standards.

---

## Design Philosophy: Schema Binding vs. Blind Lookup

Most C INI libraries use an on-demand dictionary lookup:
```c
// Traditional Approach: Verbose, error-prone, silently ignores typos
IniDict *dict = ini_load("config.ini");
int port = ini_get_int(dict, "server", "port", 8080);
const char *host = ini_get_str(dict, "server", "host", "127.0.0.1");
// If the user wrote 'prt = 9000', the typo is ignored, and 8080 is silently used!
```

`libini` adopts the schema-binding philosophy shared by modern CLI parsers:
```c
// libini Approach: Declarative, safe, fail-fast
AppConfig cfg = { .host = "127.0.0.1", .port = 8080 }; // Default values

IniParser p;
ini_init(&p);
ini_bind_str(&p, "server", "host", cfg.host, sizeof(cfg.host), false, NULL);
ini_bind_int(&p, "server", "port", &cfg.port, true, NULL); // required!

IniStatus status = ini_parse_file(&p, "config.ini");
// Typos, invalid numbers, and missing required keys halt parsing with exact error messages!
```

---

## Quick Start

### 1. Include and Define Implementation
In **exactly one** `.c` file, define `LIBINI_IMPLEMENTATION` before including the header:

```c
#define LIBINI_IMPLEMENTATION
#include "libini.h"
```

In all other translation units, simply include the header normally:
```c
#include "libini.h"
```

### 2. Basic Example

```c
#include <stdio.h>
#define LIBINI_IMPLEMENTATION
#include "libini.h"

typedef struct {
    char host[64];
    int port;
    bool enabled;
} Config;

int main(void) {
    // 1. Set fallback defaults
    Config cfg = { .host = "127.0.0.1", .port = 8080, .enabled = false };

    // 2. Initialize parser
    IniParser p;
    ini_init(&p);

    // 3. Bind schema
    ini_bind_str(&p, "network", "host", cfg.host, sizeof(cfg.host), false, NULL);
    ini_bind_int(&p, "network", "port", &cfg.port, true, NULL);
    ini_bind_bool(&p, "network", "enabled", &cfg.enabled, false);

    // 4. Parse file
    if (ini_parse_file(&p, "config.ini") != INI_OK) {
        fprintf(stderr, "Configuration error: %s\n", ini_get_error(&p));
        ini_free(&p);
        return 1;
    }

    printf("Server listening at %s:%d (enabled: %s)\n",
           cfg.host, cfg.port, cfg.enabled ? "yes" : "no");

    ini_free(&p);
    return 0;
}
```

---

## INI Format & Syntax Rules

`libini` enforces standard, intuitive formatting conventions:

```ini
# Global section (keys defined before any [section] header)
app_version = "1.0.4"
debug_mode  = true

[database]
# Both '=' and ':' are accepted delimiters
host: localhost
port = 5432

# Quotes preserve trailing spaces, semicolons, and hashes
password = "super;secret#pwd "

# Inline comments start with ';' or '#'
max_pool = 20 ; Maximum active worker connections
```

- **Sections:** Declared with `[section_name]`. Case-insensitive. Keys prior to the first section belong to the global section (`section = NULL`).
- **Delimiters:** Both `=` and `:` are supported.
- **Comments:** Full-line and inline comments begin with `;` or `#`. Characters inside single (`'...'`) or double (`"..."`) quotes are **not** treated as comments.
- **Booleans:** Case-insensitive parsing. Truthy: `true`, `1`, `yes`, `on`. Falsy: `false`, `0`, `no`, `off`.
- **Strings:** Quoted strings have their outer quotes stripped automatically.

---

## API Reference

### 1. Lifecycle Management

#### `void ini_init(IniParser *p)`
Initializes an `IniParser` instance, allocating the initial internal rule table.
- **Parameters:** `p` – Pointer to the parser context.

#### `void ini_free(IniParser *p)`
Releases dynamic memory allocated for schema tracking and resets the parser fields. Target buffers bound to the parser remain unaffected.
- **Parameters:** `p` – Pointer to the parser context.

#### `const char *ini_get_error(const IniParser *p)`
Retrieves the human-readable error message explaining why parsing or validation failed.
- **Returns:** Null-terminated error description string.

---

### 2. Schema Binding Helpers

All binding functions register an expected key in the schema. If `section` is `NULL` or `""`, the key is bound to the **global** section.

#### `void ini_bind_bool(IniParser *p, const char *section, const char *key, bool *target, bool required)`
Binds a `bool` variable.
- Supported text representations: `true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0`.

#### `void ini_bind_int(IniParser *p, const char *section, const char *key, int *target, bool required, const char *choices)`
Binds an `int` variable. Value is converted using `strtol()` with base 10.
- `choices`: Optional comma-separated valid integers (e.g., `"80,443,8080"`), or `NULL`.

#### `void ini_bind_float(IniParser *p, const char *section, const char *key, double *target, bool required, const char *choices)`
Binds a `double` variable. Value is converted using `strtod()`.
- `choices`: Optional comma-separated valid floats (e.g., `"1.0,1.5,2.0"`), or `NULL`.

#### `void ini_bind_char(IniParser *p, const char *section, const char *key, char *target, bool required, const char *choices)`
Binds a single character. Rejects input strings longer than one character.
- `choices`: String containing valid characters (e.g., `"abc"` or `"a,b,c"`), or `NULL`.

#### `void ini_bind_str(IniParser *p, const char *section, const char *key, char *target, size_t max_size, bool required, const char *choices)`
Binds a fixed-capacity `char` buffer. Copies up to `max_size - 1` characters and guarantees null-termination.
- `max_size`: Size of the target buffer in bytes (usually `sizeof(target)`).
- `choices`: Optional comma-separated allowed string choices (e.g., `"dev,staging,prod"`), or `NULL`.

---

### 3. Parsing Functions

#### `IniStatus ini_parse_file(IniParser *p, const char *filename)`
Reads an INI file from disk and parses it against the registered schema.
- **Returns:** `INI_OK (0)` on success, or an error status code on failure.

#### `IniStatus ini_parse_string(IniParser *p, const char *content)`
Parses an INI document directly from a null-terminated memory buffer.
- **Returns:** `INI_OK (0)` on success, or an error status code on failure.

---

### 4. Status Codes

| Code | Constant | Meaning |
| :--- | :--- | :--- |
| `0` | `INI_OK` | Parsing completed successfully; all constraints satisfied. |
| `1` | `INI_ERR_FILE_IO` | Could not open or read the specified file. |
| `2` | `INI_ERR_SYNTAX` | Malformed line, unclosed section header, or missing delimiter. |
| `3` | `INI_ERR_UNKNOWN_KEY` | Key found in file is not defined in the schema (catches typos). |
| `4` | `INI_ERR_INVALID_VALUE` | Conversion failed (e.g., `"abc"` for an int) or choice not allowed. |
| `5` | `INI_ERR_MISSING_REQUIRED` | A field marked `required = true` was not present in the INI file. |
| `6` | `INI_ERR_ALLOC` | Memory allocation failed during parser table expansion. |

---

## Comprehensive Example

### Configuration File (`app.ini`)

```ini
# Global Section
environment = production

[server]
host     = "0.0.0.0"
port     = 8443
workers  = 8
timeout  = 30.5
tls_mode = s ; 's' = strict, 'm' = mutual, 'd' = disabled

[database]
engine   = postgres
host     = 10.0.0.5
port     = 5432
password = "secret;pass#123" ; Quoted comment characters are preserved
```

### Implementation (`main.c`)

```c
#include <stdio.h>
#define LIBINI_IMPLEMENTATION
#include "libini.h"

typedef struct {
    char environment[32];
    struct {
        char host[64];
        int port;
        int workers;
        double timeout;
        char tls_mode;
    } server;
    struct {
        char engine[16];
        char host[64];
        int port;
        char password[64];
    } db;
} AppConfig;

int main(void) {
    // 1. Initialize default values
    AppConfig cfg = {
        .environment     = "development",
        .server.host     = "127.0.0.1",
        .server.port     = 8080,
        .server.workers  = 4,
        .server.timeout  = 15.0,
        .server.tls_mode = 'd',
        .db.engine       = "sqlite",
        .db.host         = "localhost",
        .db.port         = 5432,
        .db.password     = ""
    };

    // 2. Initialize the parser
    IniParser p;
    ini_init(&p);

    // 3. Register bindings
    ini_bind_str(&p, NULL, "environment", cfg.environment, sizeof(cfg.environment), false, "development,staging,production");

    // [server]
    ini_bind_str(&p, "server", "host", cfg.server.host, sizeof(cfg.server.host), false, NULL);
    ini_bind_int(&p, "server", "port", &cfg.server.port, true, NULL); // required!
    ini_bind_int(&p, "server", "workers", &cfg.server.workers, false, NULL);
    ini_bind_float(&p, "server", "timeout", &cfg.server.timeout, false, NULL);
    ini_bind_char(&p, "server", "tls_mode", &cfg.server.tls_mode, false, "s,m,d");

    // [database]
    ini_bind_str(&p, "database", "engine", cfg.db.engine, sizeof(cfg.db.engine), true, "postgres,mysql,sqlite");
    ini_bind_str(&p, "database", "host", cfg.db.host, sizeof(cfg.db.host), true, NULL);
    ini_bind_int(&p, "database", "port", &cfg.db.port, false, NULL);
    ini_bind_str(&p, "database", "password", cfg.db.password, sizeof(cfg.db.password), true, NULL);

    // 4. Parse and validate
    if (ini_parse_file(&p, "app.ini") != INI_OK) {
        fprintf(stderr, "Configuration error:\n  --> %s\n", ini_get_error(&p));
        ini_free(&p);
        return 1;
    }

    ini_free(&p);

    // 5. Use the validated configuration
    printf("Application loaded in [%s] mode\n", cfg.environment);
    printf("Server listening at %s:%d (Workers: %d, Timeout: %.1fs, TLS: %c)\n",
           cfg.server.host, cfg.server.port, cfg.server.workers, cfg.server.timeout, cfg.server.tls_mode);
    printf("Connected to %s at %s:%d\n", cfg.db.engine, cfg.db.host, cfg.db.port);

    return 0;
}
```

---

## Error Handling & Strict Validation

`libini` fails fast with actionable error messages that include the offending line number:

| Scenario in INI file | Error Message Produced |
| :--- | :--- |
| Typo in key: `prt = 8443` | `line 6: unknown key 'prt' in section '[server]'` |
| Invalid choice: `engine = oracle` | `line 12: invalid choice 'oracle' for key 'engine' (allowed: [postgres,mysql,sqlite])` |
| Invalid integer: `port = eight` | `line 6: invalid integer value 'eight' for key 'port'` |
| Character too long: `tls_mode = strict` | `line 9: invalid char value 'strict' for key 'tls_mode' (expected 1 char)` |
| Missing mandatory key: `password` omitted | `missing mandatory option: [database] password` |

