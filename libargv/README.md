# libargv

A single-header C99 command-line argument parser with direct variable binding, validation rules, and zero dependencies.

---

## Features

- **Single-header**: Drop `libargv.h` into your project and define `LIBARGV_IMPLEMENTATION` in one source file.
- **Direct variable binding**: Binds parsed arguments directly to `bool`, `char`, `int`, `double`, and `const char*` pointers.
- **Explicit defaults**: Set initial values and help-text defaults in one call via `argv_set_default`.
- **Validation & constraints**: Whitelist choices (`"json,csv,yaml"`), mutual exclusion groups, and cross-flag dependencies.
- **POSIX & GNU syntax**: Short-flag clustering (`-xvf`), inline values (`--port=8080`), attached values (`-p8080`), negative numbers (`--offset -12`), and stdin/stdout markers (`-o -`).
- **Standard `--` delimiter**: Treats all tokens following `--` as positional arguments.
- **Variadic rest arguments**: Collect trailing arguments into a dynamically allocated array.
- **Environment variable fallback**: Reads unset options from custom or prefixed environment variables (`APP_PORT`).
- **Redirectable output**: Send help and version output to any `FILE*` stream for automated testing.
- **Const-correct API**: Fully compatible with `const char *const *argv` without qualifier warnings.

---

## Installation

Add `libargv.h` to your source tree.

In **one** C source file, define the implementation macro before including the header:

```c
#define LIBARGV_IMPLEMENTATION
#include "libargv.h"
```

In all other files where the parser is needed, include the header normally:

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
    bool dry_run;
    bool compress;
    int verbosity = 0;
    int port;
    int offset;
    char mode;
    const char *format;
    const char *output;
    bool encrypt;
    bool plain;
    const char *key = NULL;

    const char *input_file = NULL;
    const char **extra_files = NULL;
    size_t extra_count = 0;

    CliParser p;
    argv_init(&p, argv[0]);

    argv_set_version(&p, "1.2.0", "V", "Build 2026.1");
    argv_set_env_prefix(&p, "APP");

    // Option flags
    argv_bind_bool(&p, "d", "dry-run", &dry_run, "Simulate execution");
    argv_bind_bool(&p, "c", "compress", &compress, "Enable compression");
    argv_bind_count(&p, "v", "verbose", &verbosity, "Increase verbosity level (-v, -vv)");
    argv_bind_int(&p, "p", "port", &port, false, NULL, "Server listen port");
    argv_bind_int(&p, NULL, "offset", &offset, false, NULL, "Memory offset delta");
    argv_bind_char(&p, "m", "mode", &mode, false, "r,w,a", "File access mode");
    argv_bind_str(&p, "f", "format", &format, false, "json,csv,yaml", "Data format");
    argv_bind_str(&p, "o", "output", &output, false, NULL, "Output destination ('-' for stdout)");
    argv_bind_bool(&p, "e", "encrypt", &encrypt, "Encrypt payload");
    argv_bind_bool(&p, NULL, "plain", &plain, "Disable encryption");
    argv_bind_str(&p, "k", "key", &key, false, NULL, "Encryption key");

    // Defaults (sets target value and registers help text)
    argv_set_default(&p, "dry-run", "false");
    argv_set_default(&p, "compress", "true");
    argv_set_default(&p, "port", "8080");
    argv_set_default(&p, "offset", "0");
    argv_set_default(&p, "mode", "r");
    argv_set_default(&p, "format", "json");
    argv_set_default(&p, "output", "-");

    // Choice documentation
    argv_set_choice_help(&p, "format", "json", "Structured JSON format");
    argv_set_choice_help(&p, "format", "csv", "Comma-separated values");
    argv_set_choice_help(&p, "format", "yaml", "Human-readable YAML");

    // Rules
    argv_add_exclusive(&p, "encrypt, plain");
    argv_add_dependency(&p, "key", "encrypt");

    // Positional arguments
    argv_bind_pos(&p, "input", &input_file, true, NULL, "Input file path");
    argv_bind_rest(&p, "files", &extra_files, &extra_count, false, NULL, "Additional files");

    CliStatus status = argv_parse(&p, argc, (const char *const *)argv);

    if (status == ARGV_HELP_REQUESTED || status == ARGV_VERSION_REQUESTED) {
        argv_free(&p);
        return 0;
    }

    if (status != ARGV_OK) {
        fprintf(stderr, "Error: %s\n", argv_get_error(&p));
        argv_free(&p);
        return 1;
    }

    printf("Input: %s, Port: %d, Mode: %c, Output: %s\n", input_file, port, mode, output);

    argv_free(&p);
    return 0;
}
```

Compile with standard compiler flags:

```bash
gcc -Wall -Wextra -Wpedantic main.c -o app
```

---

## Supported Syntax

| Syntax | Example | Description |
|---|---|---|
| No-dash binding | `argv_bind_bool(p, "v", "verbose", ...)` | Accepts options with or without leading dashes. |
| Standard flags | `-p 8080`, `--port 8080` | Space-separated flag values. |
| Inline values | `--port=8080` | Key-value flag syntax. |
| Boolean negation | `--no-compress`, `--compress=false` | Disables a boolean switch. |
| Short clusters | `-xvf` | Expands to `-x -v -f`. |
| Attached values | `-p8080`, `-I/usr/include` | Value attached directly to the short flag. |
| Counter flags | `-v`, `-vv`, `-vvv` | Increments an `int` counter. |
| Negative numbers | `--offset -12` | Negative numeric literals are not parsed as flags. |
| Stdin/stdout flag | `-o -` | Preserves `-` as a literal value. |
| Delimiter | `-- -file.txt` | Treats everything after `--` as positional arguments. |

---

## Subcommands

Multi-command CLIs can be structured with a root parser validating subcommands, followed by dispatch to specialized sub-parsers:

```c
int main(int argc, char **argv) {
    const char *cmd = NULL;

    CliParser root;
    argv_init(&root, "git");
    argv_bind_pos(&root, "command", &cmd, true, "commit,clone", "Subcommand to run");
    argv_set_choice_help(&root, "command", "commit", "Record repository changes");
    argv_set_choice_help(&root, "command", "clone", "Clone remote repository");

    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        argv_parse(&root, argc, (const char *const *)argv);
        argv_free(&root);
        return 0;
    }

    const char *const check_argv[] = {argv[0], argv[1], NULL};
    if (argv_parse(&root, 2, check_argv) != ARGV_OK) {
        fprintf(stderr, "%s\n", argv_get_error(&root));
        argv_free(&root);
        return 1;
    }

    int sub_argc = argc - 1;
    const char *const *sub_argv = (const char *const *)&argv[1];

    if (strcmp(cmd, "commit") == 0) {
        run_commit(sub_argc, sub_argv);
    }

    argv_free(&root);
    return 0;
}
```

---

## API Reference

### Initialization & Cleanup

```c
void argv_init(CliParser *p, const char *program_name);
void argv_free(CliParser *p);
const char *argv_get_error(const CliParser *p);
void argv_set_output(CliParser *p, FILE *out);
void argv_set_version(CliParser *p, const char *version, const char *sh, const char *extra_info);
void argv_set_env_prefix(CliParser *p, const char *prefix);
```

### Option Binding

```c
void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);
void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);

void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);
void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);
```

### Defaults, Rules & Documentation

```c
void argv_set_default(CliParser *p, const char *flag_or_name, const char *default_val);
void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var);
bool argv_set_choice_help(CliParser *p, const char *flag_or_name, const char *choice, const char *help);
void argv_add_exclusive(CliParser *p, const char *flags_group);
void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags);
```

### Execution

```c
CliStatus argv_parse(CliParser *p, int argc, const char *const *argv);
void argv_print_help(const CliParser *p);
```

**Status codes:**
- `ARGV_OK`
- `ARGV_HELP_REQUESTED`
- `ARGV_VERSION_REQUESTED`
- `ARGV_ERR_UNKNOWN_FLAG`
- `ARGV_ERR_MISSING_VALUE`
- `ARGV_ERR_INVALID_VALUE`
- `ARGV_ERR_MISSING_REQUIRED`
- `ARGV_ERR_MUTUAL_EXCLUSION`
- `ARGV_ERR_DEPENDENCY`
- `ARGV_ERR_ALLOC`
