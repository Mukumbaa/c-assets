# libargv

A lightweight, robust, single-header CLI argument parser for C99. No external dependencies, no build systems to configure—just drop the header into your project and bind your variables directly using pointers.

Inspired by Go's `flag` package and Python's `argparse`, `libargv` bridges the gap between raw `getopt` and heavyweight CLI frameworks.

---

## Why use this?

Most C argument parsers are either bare-bones wrappers around POSIX `getopt` (cumbersome, manual type conversions, poor error handling) or heavy frameworks involving dozens of source files and complex dynamic allocations.

`libargv` offers the best of both worlds:
- **Single-header (STB-style)**: Drop `libargv.h` into your tree and define `LIBARGV_IMPLEMENTATION` in one `.c` file.
- **Direct variable binding**: Point the parser to your `bool`, `char`, `int`, `double`, or `const char*` targets; it handles parsing, bounds checking, and assignment.
- **Boolean negation out of the box**: Supports `--flag`, `--no-flag`, and explicit inline booleans (`--flag=false`, `--flag=true`, `1`, `0`, `yes`, `no`, `on`, `off`). Automatically formatted as `--[no-]flag` in help screens.
- **Automatic default values**: Displays existing defaults (`[default: ...]`) in help screens for strings, integers, floats, characters, and `true` booleans.
- **Global environment prefix**: Automatically maps flags to environment variables using `argv_set_env_prefix` (e.g., `--dry-run` maps to `APP_DRY_RUN`) with support for explicit overrides via `argv_set_env`.
- **Testable & redirectable output**: Re-route help and version output to any `FILE*` stream (`argv_set_output`) for clean unit testing without polluting `stdout`.
- **Short flag clustering**: Bundles short flags seamlessly (e.g., `-xvf archive.tar.gz` or `-vvv`).
- **Attached short values & negative numbers**: Handles `-I/usr/include`, `-p8080`, negative numeric arguments (`--offset -12`), and the standard `--` positional delimiter.
- **Documented choices**: Constrain inputs to allowed choices and attach per-choice documentation via `argv_set_choice_help`.
- **Advanced logical rules**: Enforce group-based mutual exclusion (e.g. `"json,xml,ini"`) and multi-flag dependencies with upfront validation.
- **Variadic positional arguments**: Collect trailing arguments into a dynamically managed array (`argv_bind_rest`).
- **Null & overflow safety**: Protects against integer overflows (`INT_MIN`/`INT_MAX`), checks for NULL targets, and supports clean re-parsing across test suites.

---

## Installation

Download `libargv.h` and place it in your project.

In **exactly one** `.c` file (typically `main.c`), define `LIBARGV_IMPLEMENTATION` before including the header:

```c
#define LIBARGV_IMPLEMENTATION
#include "libargv.h"
```

In any other source file where you need parser declarations, include it normally:

```c
#include "libargv.h"
```

---

## Quick Example

```c
#include <stdio.h>
#include <stdbool.h>

#define LIBARGV_IMPLEMENTATION
#include "libargv.h"

int main(int argc, char **argv) {
    // 1. Declare variables with default values
    bool dry_run = false;
    bool compress = true; // Will show [default: true] in help
    int verbosity = 0;
    char mode = 'r';
    int port = 8080;      // Will show [default: 8080] in help
    const char *format = "json";
    const char *token = NULL;
    const char *key = NULL;
    bool encrypt = false;
    bool plain = false;
    const char *input_file = NULL;
    const char **extra_files = NULL;
    size_t extra_count = 0;

    // 2. Initialize parser
    CliParser parser;
    argv_init(&parser, argv[0]);

    // Optional: configure version banner (-V / --version)
    argv_set_version(&parser, "1.2.0", "V", "Build 2026.1 - Open Source Tool");

    // Optional: map unset flags to environment variables automatically (e.g. DATAX_PORT)
    argv_set_env_prefix(&parser, "DATAX");

    // 3. Bind flags (leading dashes are optional during binding)
    argv_bind_bool(&parser, "d", "dry-run", &dry_run, "Simulate execution without modifying files");
    argv_bind_bool(&parser, "c", "compress", &compress, "Enable payload compression");
    argv_bind_count(&parser, "v", "verbose", &verbosity, "Increase verbosity level (-v, -vv, -vvv)");
    argv_bind_char(&parser, "m", "mode", &mode, false, "r,w,a", "File access mode");
    argv_bind_int(&parser, "p", "port", &port, false, NULL, "Server listen port");
    argv_bind_str(&parser, "f", "format", &format, false, "json,csv,yaml", "Data output format");
    argv_bind_str(&parser, NULL, "token", &token, false, NULL, "Authentication token");
    argv_bind_bool(&parser, "e", "encrypt", &encrypt, "Enable encryption");
    argv_bind_bool(&parser, NULL, "plain", &plain, "Force unencrypted plain text");
    argv_bind_str(&parser, "k", "key", &key, false, NULL, "Encryption secret key");

    // 4. Document choices
    argv_set_choice_help(&parser, "format", "json", "Structured JSON format");
    argv_set_choice_help(&parser, "format", "csv", "Comma-separated values");
    argv_set_choice_help(&parser, "format", "yaml", "Human-readable YAML");

    // 5. Validation rules
    argv_add_exclusive(&parser, "encrypt, plain"); // Mutually exclusive flags
    argv_add_dependency(&parser, "key", "encrypt"); // --key requires --encrypt

    // 6. Positional arguments
    argv_bind_pos(&parser, "input", &input_file, true, NULL, "Primary input file");
    argv_bind_rest(&parser, "files", &extra_files, &extra_count, false, NULL, "Additional files to process");

    // 7. Parse argv
    CliStatus status = argv_parse(&parser, argc, argv);

    // Clean exit on help or version request
    if (status == ARGV_HELP_REQUESTED || status == ARGV_VERSION_REQUESTED) {
        argv_free(&parser);
        return 0;
    }

    if (status != ARGV_OK) {
        fprintf(stderr, "Error: %s\n", argv_get_error(&parser));
        argv_free(&parser);
        return 1;
    }

    printf("Running with file: %s on port %d (dry-run: %s, compress: %s)\n",
           input_file, port, dry_run ? "yes" : "no", compress ? "yes" : "no");

    // Clean up internal allocations
    argv_free(&parser);
    return 0;
}
```

Compile with any standard C99+ compiler:

```bash
gcc -Wall -Wextra main.c -o main
```

---

## Supported Syntax

| Syntax | Example | Description |
|---|---|---|
| **No-dash binding** | `argv_bind_bool(p, "v", "verbose", ...)` | Accepts options with or without leading hyphens. |
| **Standard flags** | `-f json`, `--format json` | Traditional space-separated flag values. |
| **Inline values** | `--format=json`, `--port=8080` | Standard key-value syntax. |
| **Boolean negation** | `--no-dry-run`, `--dry-run=false` | Toggles or explicitly disables boolean flags. |
| **Short clusters** | `-vcd` | Equivalent to passing `-v -c -d`. |
| **Attached short values** | `-I/usr/include`, `-p8080` | Short flags with immediate attached values. |
| **Counters** | `-v`, `-vv`, `-vvv` | Increments an `int` counter each time it is passed. |
| **Negative numbers** | `--offset -12`, `-t -0.5` | Negative numbers are not mistaken for unknown flags. |
| **Value choices** | `choices: "json,csv,yaml"` | Validates values against comma- or pipe-delimited choices. |
| **Documented choices** | `argv_set_choice_help(...)` | Displays indented descriptions for each allowed choice in help screens. |
| **Default hints** | `[default: 8080]` | Help screens automatically show active non-zero / true defaults. |
| **Environment fallback** | `DATAX_PORT=9000 ./app` | Automatically resolves missing flags via `argv_set_env` or `argv_set_env_prefix`. |
| **Exclusive groups** | `--encrypt --plain` *(rejected)* | Forbids using conflicting options defined in an exclusive group. |
| **Flag dependencies** | `--key 123` *(without `--encrypt`)* | Enforces prerequisite options before a dependent flag can be set. |
| **Variadic arguments** | `input.txt a.c b.c c.c` | Trailing positional arguments collected into `const char**`. |
| **Double-dash `--`** | `-- -weird-file.txt` | Treats everything following `--` as a positional argument. |

---

## Subcommands (Git-style CLI)

You don't need complicated command hierarchies or external libraries to implement multi-command tools like `git` or `docker`. 

With `libargv`, you can build clean subcommands with **zero dynamic allocations**:
1. The **root parser** validates the command name against allowed choices and documents them via `argv_set_choice_help`.
2. A simple `strcmp` dispatcher passes the tail of `argv` directly to a dedicated subcommand parser.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define LIBARGV_IMPLEMENTATION
#include "libargv.h"

static void run_commit(int argc, char **argv);
static void run_clone(int argc, char **argv);

int main(int argc, char **argv) {
    const char *cmd = NULL;

    // 1. Root Parser: validates and documents top-level subcommands
    CliParser root;
    argv_init(&root, "git");
    argv_set_version(&root, "2.43.0", "V", "A Git CLI mock in C");
    argv_bind_pos(&root, "command", &cmd, true, "commit,clone", "Git subcommand to execute");
    argv_set_choice_help(&root, "command", "commit", "Record changes to the repository");
    argv_set_choice_help(&root, "command", "clone", "Clone a repository into a new directory");

    // Handle general --help, --version, or missing arguments
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 ||
        strcmp(argv[1], "-V") == 0 || strcmp(argv[1], "--version") == 0) {
        CliStatus st = argv_parse(&root, argc, argv);
        argv_free(&root);
        return (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) ? 0 : 1;
    }

    // Validate that the subcommand is recognized
    char *root_check_argv[] = {argv[0], argv[1]};
    if (argv_parse(&root, 2, root_check_argv) != ARGV_OK) {
        fprintf(stderr, "git: %s\n", argv_get_error(&root));
        argv_free(&root);
        return 1;
    }

    // Forward the remaining arguments to the dedicated subcommand parser
    // &argv[1] has argv[1] ("commit") as the command name, perfectly serving as argv[0]
    int sub_argc = argc - 1;
    char **sub_argv = &argv[1];

    if (strcmp(cmd, "commit") == 0) {
        run_commit(sub_argc, sub_argv);
    } else if (strcmp(cmd, "clone") == 0) {
        run_clone(sub_argc, sub_argv);
    }

    argv_free(&root);
    return 0;
}

static void run_commit(int argc, char **argv) {
    const char *message = NULL;
    bool all = false;

    CliParser p;
    argv_init(&p, "git commit");
    argv_bind_str(&p, "m", "message", &message, true, NULL, "Commit message");
    argv_bind_bool(&p, "a", "all", &all, "Automatically stage modified files");

    CliStatus st = argv_parse(&p, argc, argv);
    if (st == ARGV_HELP_REQUESTED) {
        argv_free(&p);
        return;
    }
    if (st != ARGV_OK) {
        fprintf(stderr, "commit: %s\n", argv_get_error(&p));
        argv_free(&p);
        exit(1);
    }
    printf("Committed: %s (stage all: %s)\n", message, all ? "true" : "false");
    argv_free(&p);
}

static void run_clone(int argc, char **argv) {
    int depth = 0;
    const char *repo = NULL;

    CliParser p;
    argv_init(&p, "git clone");
    argv_bind_int(&p, NULL, "depth", &depth, false, NULL, "Create a shallow clone");
    argv_bind_pos(&p, "repository", &repo, true, NULL, "Repository URL");

    CliStatus st = argv_parse(&p, argc, argv);
    if (st == ARGV_HELP_REQUESTED) {
        argv_free(&p);
        return;
    }
    if (st != ARGV_OK) {
        fprintf(stderr, "clone: %s\n", argv_get_error(&p));
        argv_free(&p);
        exit(1);
    }
    printf("Cloning %s (depth: %d)\n", repo, depth);
    argv_free(&p);
}
```

#### What you get automatically:
- **Top-level help (`git --help`)**:
  ```text
  Usage: git [OPTIONS] <command>

  Positional Arguments:
    <command> <str>      Git subcommand to execute [required]
      clone: Clone a repository into a new directory
      commit: Record changes to the repository
  ```
- **Contextual subcommand help (`git commit --help`)**:
  ```text
  Usage: git commit [OPTIONS]

  Options:
    -h, --help           Display this help message
    -m, --message <str>  Commit message [required]
    -a, --all            Automatically stage modified files
  ```
---

## API Reference

### Lifecycle and Configuration

```c
void argv_init(CliParser *p, const char *program_name);
```
Initializes the parser instance. Pass `argv[0]` or your program name.

```c
void argv_free(CliParser *p);
```
Frees dynamic memory allocated by the parser (rules, choice help strings, and dynamic arrays created by `argv_bind_rest`). Always call this before exiting.

```c
void argv_set_output(CliParser *p, FILE *out);
```
Redirects output (default `stdout`) to any `FILE*` stream (e.g., `stderr`, custom file, or `tmpfile()` for unit tests).

```c
void argv_set_env_prefix(CliParser *p, const char *prefix);
```
Sets a global prefix for environment variable lookups. Unset long flags will automatically check `<PREFIX>_<FLAG_NAME>` (e.g., `--dry-run` checks `PREFIX_DRY_RUN`).

```c
const char *argv_get_error(const CliParser *p);
```
Returns a descriptive, null-terminated error string if parsing or initialization fails.

---

### Version Management

```c
void argv_set_version(CliParser *p, const char *version, const char *sh, const char *extra_info);
```
Configures version information:
- `version`: Version string (e.g. `"1.2.0"`).
- `sh`: Optional short flag (e.g. `"V"` or `"-V"`). Pass `NULL` for `--version` only.
- `extra_info`: Optional secondary metadata banner. Pass `NULL` to omit.

---

### Binding Options

Options can be registered with or without leading dashes. Passing `NULL` as a target pointer records an initialization error and prevents execution.

```c
void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);
void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);
```

#### Positional and Rest Arguments
```c
void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);
void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);
```
`argv_bind_rest` captures all remaining positional arguments into an allocated array `const char**`.

---

### Environment Variables & Validation Rules

```c
void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var);
```
Explicitly associates an environment variable with an option (takes precedence over the global prefix).

```c
void argv_add_exclusive(CliParser *p, const char *flags_group);
void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags);
```
Defines mutual exclusion groups (e.g. `"encrypt, plain"`) or prerequisites (e.g. flag `"key"` requires `"encrypt"`). Rule flag names are validated at parse time.

---

### Parsing and Status Codes

```c
CliStatus argv_parse(CliParser *p, int argc, char **argv);
```

Returns one of the following `CliStatus` codes:
- `ARGV_OK`: Parsed successfully.
- `ARGV_HELP_REQUESTED`: `-h` or `--help` handled.
- `ARGV_VERSION_REQUESTED`: `-V` or `--version` handled.
- `ARGV_ERR_UNKNOWN_FLAG`: Unregistered flag or extra positional argument.
- `ARGV_ERR_MISSING_VALUE`: A flag requiring an argument was passed without one.
- `ARGV_ERR_INVALID_VALUE`: Conversion error, invalid choice, integer overflow, or bad boolean value.
- `ARGV_ERR_MISSING_REQUIRED`: Mandatory flag or positional argument was omitted.
- `ARGV_ERR_MUTUAL_EXCLUSION`: Conflicting options from an exclusive group were supplied.
- `ARGV_ERR_DEPENDENCY`: A required dependency was not satisfied.
- `ARGV_ERR_ALLOC`: Memory allocation failed.

---

## Built-in Help System

`libargv` automatically aligns columns and formats boolean negations, default values, and environment variables:

```text
datax v1.2.0

Usage: datax [OPTIONS] <input> [files...]

Options:
  -h, --help                    Display this help message
  -V, --version                 Display version information
  -d, --[no-]dry-run            Simulate execution without modifying files
  -c, --[no-]compress           Enable payload compression [default: true]
  -v, --verbose                 Increase verbosity level (-v, -vv, -vvv)
  -m, --mode {r,w,a}            File access mode
  -p, --port <int>              Server listen port [default: 8080] [env: DATAX_PORT]
  -f, --format {json,csv,yaml}  Data output format [default: json]
                                  json: Structured JSON format
                                  csv: Comma-separated values
                                  yaml: Human-readable YAML
      --token <str>             Authentication token [env: DATAX_TOKEN]
  -e, --[no-]encrypt            Enable encryption
      --[no-]plain              Force unencrypted plain text
  -k, --key <str>               Encryption secret key

Positional Arguments:
  <input>                       Primary input file [required]
  <files...>                    Additional files to process
```

Inspect individual flags via `--help <flag_name>`:
```text
Short flag:            -p
Long flag:             --port
Value required:        Yes
Mandatory:             No
Default:               8080
Environment variable:  DATAX_PORT

Description:
  Server listen port
```
