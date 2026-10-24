# libargv

A lightweight, single-header CLI argument parser for C99. No external dependencies, no build systems to configure—just drop the header in your tree and bind your variables.

Inspired by Go's `flag` package and Python's `argparse`, `libargv` lets you bind your CLI flags directly to your stack variables using pointers.

---

## Why use this?

Most C argument parsers fall into two camps: either they wrap raw POSIX `getopt` (which gets messy quickly and doesn't handle types well), or they are heavy frameworks with tons of files, memory allocations, and boilerplate.

`libargv` aims for the middle ground:
- **Single-header (STB-style)**: Put `libargv.h` in your project and you are done.
- **Direct variable binding**: Point the parser to your `bool`, `char`, `int`, `double`, or `char*` and it handles validation and assignment.
- **Choices validation**: Restrict strings, characters, and numbers to a predefined set of allowed values (e.g. `"json,xml,yaml"`).
- **Variadic positional arguments**: Collect remaining trailing arguments (`argv_bind_rest`) into a dynamically allocated array.
- **Modern syntax out of the box**: Supports short flag stacking (`-xvf`), inline assignments (`--port=8080`), attached short values (`-I/usr/include`), negative numbers (`--offset -10`), and the standard `--` positional delimiter.
- **Clean error handling**: It never writes to `stderr` without your permission. It records what went wrong and lets your application decide how to format or log errors.
- **Auto-generated help**: Dynamic column alignment, choices preview, and automated per-flag inspection.

---

## Installation

Download `libargv.h` and include it in your project.

In **exactly one** `.c` translation unit (usually your `main.c`), define `LIBARGV_IMPLEMENTATION` before including the header to emit the implementation code:

```c
#define LIBARGV_IMPLEMENTATION
#include "libargv.h"
```

In any other file where you need to call parser functions, just include it normally:

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
    // Declare your variables with sensible defaults
    bool verbose = false;
    int verbosity_level = 0;
    char mode = 'r';
    int port = 8080;
    double timeout = 15.5;
    const char *format = "json";
    const char *input_file = NULL;
    const char **extra_files = NULL;
    size_t extra_count = 0;

    // Initialize the parser
    CliParser parser;
    argv_init(&parser, argv[0]);

    // Bind flags to variables
    argv_bind_bool(&parser, "-v", "--verbose", &verbose, "Turn on debug logs");
    argv_bind_count(&parser, "-l", "--log-level", &verbosity_level, "Repeat for higher verbosity (-l, -ll)");
    argv_bind_char(&parser, "-m", "--mode", &mode, false, "rwa", "File open mode (r, w, or a)");
    argv_bind_int(&parser, "-p", "--port", &port, false, "80,443,8080", "Port to listen on");
    argv_bind_float(&parser, "-t", "--timeout", &timeout, false, NULL, "Connection timeout in seconds");
    argv_bind_str(&parser, "-f", "--format", &format, false, "json|xml|csv", "Output format");

    // Positional arguments (ordered)
    argv_bind_pos(&parser, "input", &input_file, true, NULL, "Primary input file");
    argv_bind_rest(&parser, "extras", &extra_files, &extra_count, false, NULL, "Additional files to process");

    // Parse argv
    CliStatus status = argv_parse(&parser, argc, argv);

    if (status == ARGV_HELP_REQUESTED) {
        argv_free(&parser);
        return 0; // Help already printed to stdout
    }

    if (status != ARGV_OK) {
        fprintf(stderr, "Error: %s\n", argv_get_error(&parser));
        fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
        argv_free(&parser);
        return 1;
    }

    // Use your values directly
    printf("Starting on port %d [format: %s, mode: %c] with primary file: %s\n",
           port, format, mode, input_file);

    for (size_t i = 0; i < extra_count; i++) {
        printf(" - Extra file #%zu: %s\n", i + 1, extra_files[i]);
    }

    // Clean up internal parser allocations (also frees rest argument buffers)
    argv_free(&parser);
    return 0;
}
```

Compile with any C99+ compiler:

```bash
gcc -Wall -Wextra main.c -o main
```

---

## Supported Syntax

| Syntax | Example | Description |
|---|---|---|
| **Standard flags** | `-f json`, `--format json` | Traditional space-separated options. |
| **Inline values** | `--format=json` | Key-value style long flags. |
| **Short clusters** | `-vxl` | Equivalent to passing `-v -x -l` separately. |
| **Attached short values** | `-I/usr/include`, `-mc` | Value immediately following the short option. |
| **Counters** | `-l`, `-ll`, `-lll` | Increments an `int` counter each time it is encountered. |
| **Negative numbers** | `--offset -12`, `-t -0.5` | Won't mistake negative numbers for unrecognized flags. |
| **Value choices** | `--format yaml` *(rejected)* | Automatically checks input against comma- or pipe-delimited choices. |
| **Variadic pos args** | `input.txt a.c b.c c.c` | Rest arguments captured into `const char**`. |
| **Double-dash `--`** | `-- -weird-filename.txt` | Treats everything following `--` as a positional argument. |

---

## API Reference

### Setup and Lifecycle

```c
void argv_init(CliParser *p, const char *program_name);
```
Prepares the parser instance and allocates initial storage. Pass `argv[0]` as `program_name`.

```c
void argv_free(CliParser *p);
```
Frees dynamic memory allocated by the parser, including any array memory allocated for `argv_bind_rest`. Always call this before exiting.

```c
const char *argv_get_error(const CliParser *p);
```
Returns a pointer to a descriptive, null-terminated error string if `argv_parse` fails.

```c
void argv_print_help(const CliParser *p);
```
Manually prints the generated usage and options screen.

---

### Binding Options

Options can be marked `required` and constrained by `choices`. Set `choices` to `NULL` to disable value restriction. Choices can be separated by commas (`,`)|pipes (`|`), or compact characters (e.g. `"abc"` for `char` types).

#### Boolean & Counters
```c
void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
```
Flags taking no argument; sets `target` to `true` when present.

```c
void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
```
Flags that increment an integer every time they appear (e.g. `-v`, `-vv`).

#### Typed Options
```c
void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);
```
Parses a single ASCII character. Rejects input strings longer than 1 character.

```c
void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);
```
Parses integers (via `strtol`), floats (via `strtod`), or direct string pointers (`const char*` pointing into `argv`). If `choices` is provided, values outside the set will return `ARGV_ERR_INVALID_VALUE`.

#### Positional and Rest Arguments
```c
void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);
```
Registers an ordered positional argument. Positional arguments are resolved in the order they are registered.

```c
void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);
```
Captures all remaining positional arguments into a dynamically allocated array `const char **`. The total number of captured elements is stored in `*count`. The allocated array is automatically cleaned up when `argv_free(p)` is called.

---

### Parsing and Status Codes

```c
CliStatus argv_parse(CliParser *p, int argc, char **argv);
```

Returns one of the following `CliStatus` values:

- `ARGV_OK`: Everything parsed cleanly.
- `ARGV_HELP_REQUESTED`: `-h` or `--help` was encountered. Usage text has already been printed to `stdout`.
- `ARGV_ERR_UNKNOWN_FLAG`: Passed a flag or unexpected positional argument that was never bound.
- `ARGV_ERR_MISSING_VALUE`: A flag expecting a value was left empty.
- `ARGV_ERR_INVALID_VALUE`: Failed conversion (e.g. invalid integer/float/char) or value violated `choices`.
- `ARGV_ERR_MISSING_REQUIRED`: An option or positional argument marked `required` was not found.
- `ARGV_ERR_ALLOC`: Internal memory allocation failed.

---

## Built-in Help System

`libargv` hooks into `-h` and `--help` automatically:

1. **General help**: Running `./mytool --help` lists the usage line, all options aligned dynamically with choices/type hints, and registered positional arguments.
2. **Flag inspection**: Running `./mytool --help --port` (or `./mytool -p -h`) displays detailed documentation, mandatory status, choices, and requirements specifically for that option.
