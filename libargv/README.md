# libargv

A lightweight, single-header CLI argument parser for C99. No external dependencies, no build systems to configure—just drop the header in your tree and bind your variables.

Inspired by Go's `flag` package and Python's `argparse`, `libargv` lets you bind your CLI flags directly to your stack variables using pointers.

---

## Why use this?

Most C argument parsers fall into two camps: either they wrap raw POSIX `getopt` (which gets messy quickly and doesn't handle types well), or they are heavy frameworks with tons of files, memory allocations, and boilerplate.

`libargv` aims for the middle ground:
- **Single-header (STB-style)**: Put `libargv.h` in your project and you are done.
- **Direct variable binding**: Point the parser to your `bool`, `char`, `int`, `double`, or `char*` and it handles validation and assignment.
- **Flag normalization**: Pass `"v"` and `"verbose"` or `"-v"` and `"--verbose"`—the parser automatically normalizes prefixes for you.
- **Documented choices**: Restrict values to allowed choices (`choices`) and document each option with custom descriptions using `argv_set_choice_help`.
- **Environment variable fallback**: Seamlessly read missing values from environment variables (`argv_set_env`).
- **Advanced logical rules**: Enforce group-based mutual exclusion (e.g. `"json,xml,ini"`) and multi-flag dependencies (e.g. `--pass` requiring both `--user` and `--mail`).
- **Variadic positional arguments**: Collect remaining trailing arguments (`argv_bind_rest`) into a dynamically managed array.
- **Modern syntax out of the box**: Supports short flag stacking (`-xvf`), inline assignments (`--port=8080`), attached short values (`-I/usr/include`), negative numbers (`--offset -10`), and the standard `--` positional delimiter.
- **Clean error handling**: It never writes to `stderr` without your permission. It records what went wrong and lets your application decide how to format or log errors.
- **Auto-generated help**: Dynamic column alignment, detailed choices breakdown, environment variable hints, and automated per-flag inspection.

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
    char radix = 'd';
    int port = 8080;
    const char *format = "json";
    const char *user = NULL;
    const char *pass = NULL;
    const char *input_file = NULL;
    const char **extra_files = NULL;
    size_t extra_count = 0;

    // Initialize the parser
    CliParser parser;
    argv_init(&parser, argv[0]);

    // Bind flags: prefixes ('-' and '--') are completely optional!
    argv_bind_bool(&parser, "v", "verbose", &verbose, "Turn on debug logs");
    argv_bind_char(&parser, "t", "radix", &radix, false, "d,x,o", "Offset output radix");
    argv_bind_str(&parser, "f", "format", &format, false, "json,xml,csv", "Output format");
    argv_bind_int(&parser, "p", "port", &port, false, NULL, "Server listen port");
    argv_bind_str(&parser, "u", "user", &user, false, NULL, "Admin username");
    argv_bind_str(&parser, NULL, "pass", &pass, false, NULL, "Admin password");

    // Document individual choices for flags
    argv_set_choice_help(&parser, "radix", "d", "decimal offset");
    argv_set_choice_help(&parser, "radix", "x", "hexadecimal offset");
    argv_set_choice_help(&parser, "radix", "o", "octal offset");

    argv_set_choice_help(&parser, "format", "json", "Structured JSON output");
    argv_set_choice_help(&parser, "format", "xml", "Standard XML format");
    argv_set_choice_help(&parser, "format", "csv", "Comma-separated values");

    // Positional arguments (ordered)
    argv_bind_pos(&parser, "input", &input_file, true, NULL, "Primary input file");
    argv_bind_rest(&parser, "extras", &extra_files, &extra_count, false, NULL, "Additional files");

    // 1. Environment fallback: if --port is not supplied, look up $PORT
    argv_set_env(&parser, "port", "PORT");

    // 2. Multi-flag dependency: --pass requires --user to be set
    argv_add_dependency(&parser, "pass", "user");

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

    printf("Running on port %d [radix: %c, format: %s] with file: %s\n",
           port, radix, format, input_file);

    for (size_t i = 0; i < extra_count; i++) {
        printf(" - Extra file #%zu: %s\n", i + 1, extra_files[i]);
    }

    // Clean up internal parser allocations
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
| **No-dash registration** | `argv_bind_bool(p, "v", "verbose", ...)` | Registrations accept flag names with or without leading hyphens. |
| **Standard flags** | `-f json`, `--format json` | Traditional space-separated options. |
| **Inline values** | `--format=json` | Key-value style long flags. |
| **Short clusters** | `-vxl` | Equivalent to passing `-v -x -l` separately. |
| **Attached short values** | `-I/usr/include`, `-p8080` | Value immediately following the short option. |
| **Counters** | `-v`, `-vv`, `-vvv` | Increments an `int` counter each time it is encountered. |
| **Negative numbers** | `--offset -12`, `-t -0.5` | Won't mistake negative numbers for unrecognized flags. |
| **Value choices** | `--format yaml` *(rejected)* | Automatically checks input against comma- or pipe-delimited choices. |
| **Documented choices** | `argv_set_choice_help(...)` | Documents what each choice does; displays indented descriptions in help screens. |
| **Environment fallback** | `PORT=9000 ./app` | Automatically parses fallback environment variables when flags are omitted. |
| **Exclusive groups** | `--json --xml` *(rejected)* | Forbids using conflicting options defined in an exclusive group. |
| **Flag dependencies** | `--pass 123` *(without `--user`)* | Enforces prerequisite options before a dependent flag can be set. |
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
Frees dynamic memory allocated by the parser, including rules, choice descriptions, and dynamic array memory from `argv_bind_rest`. Always call this before exiting.

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

Options can be registered with or without leading dashes (`"v"` or `"-v"`, `"verbose"` or `"--verbose"`). 

Set `choices` to `NULL` to disable value restriction. Choices can be separated by commas (`,`), pipes (`|`), or compact characters (e.g. `"abc"` for `char` types).

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
void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);
```
Parses characters, integers (via `strtol`), floats (via `strtod`), or direct string pointers (`const char*` pointing into `argv`). If `choices` is provided, values outside the set will return `ARGV_ERR_INVALID_VALUE`.

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

### Documenting Choices

```c
bool argv_set_choice_help(CliParser *p, const char *flag_or_name, const char *choice, const char *help);
```
Assigns a descriptive help string to an individual allowed choice. 
- Verifies that `flag_or_name` exists and that `choice` is an allowed value declared in `choices`.
- If `choice` is not found in the allowed choices list, it returns `false` and records an error in `p->error`.
- Any recorded configuration error will cause subsequent `argv_parse` calls to immediately halt with `ARGV_ERR_INVALID_VALUE`.

---

### Environment Variables & Advanced Rules

```c
void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var);
```
Associates an environment variable name as a fallback for any flag or positional argument. If the option is not provided on the command line, the value of `getenv(env_var)` is parsed and validated instead.

```c
void argv_add_exclusive(CliParser *p, const char *flags_group);
```
Defines a mutually exclusive group using a comma- or pipe-separated list of flags (e.g. `"json,xml,ini"`). If more than one flag from this group is set, parsing fails with `ARGV_ERR_MUTUAL_EXCLUSION`.

```c
void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags);
```
Requires one or more prerequisite flags (comma- or pipe-separated, e.g. `"user,mail"`) whenever `flag` is specified. If any prerequisite is missing, parsing fails with `ARGV_ERR_DEPENDENCY`.

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
- `ARGV_ERR_INVALID_VALUE`: Failed conversion, value violated `choices`, or invalid choice documentation specified.
- `ARGV_ERR_MISSING_REQUIRED`: An option or positional argument marked `required` was not found.
- `ARGV_ERR_MUTUAL_EXCLUSION`: Multiple options from an exclusive group were passed together.
- `ARGV_ERR_DEPENDENCY`: A flag was provided without satisfying all prerequisite flags.
- `ARGV_ERR_ALLOC`: Internal memory allocation failed.

---

## Built-in Help System

`libargv` hooks into `-h` and `--help` automatically:

1. **General help**: Running `./mytool --help` lists the usage line, registered options, allowed choices, per-choice indented documentation, and environment variable bindings (`[env: NAME]`):
   ```text
   Options:
     -h, --help                   Display this help message
     -t, --radix {d,x,o}          Offset output radix
                                    d: decimal offset
                                    x: hexadecimal offset
                                    o: octal offset
     -p, --port <int>             Server listen port [env: PORT]
   ```
2. **Flag inspection**: Running `./mytool --help radix` (or `./mytool -t -h`) displays full details for that option, including allowed choices with aligned explanations:
   ```text
   Short flag:            -t
   Long flag:             --radix
   Value required:        Yes
   Mandatory:             No
   Allowed choices:       [d,x,o]
     d : decimal offset
     x : hexadecimal offset
     o : octal offset
   Description:           Offset output radix
   ```
