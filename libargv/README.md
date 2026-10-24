# libargv

A lightweight, single-header CLI argument parser for C99. No external dependencies, no build systems to configure—just drop the header in your tree and bind your variables.

Inspired by Go's `flag` package and Python's `argparse`, `libargv` lets you bind your CLI flags directly to your stack variables using pointers.

---

## Why use this?

Most C argument parsers fall into two camps: either they wrap raw POSIX `getopt` (which gets messy quickly and doesn't handle types well), or they are heavy frameworks with tons of files, memory allocations, and boilerplate.

`libargv` aims for the middle ground:
- **Single-header (STB-style)**: Put `libargv.h` in your project and you are done.
- **Direct variable binding**: Point the parser to your `bool`, `int`, `double`, or `char*` and it handles validation and assignment.
- **Modern syntax out of the box**: Supports short flag stacking (`-xvf`), inline assignments (`--port=8080`), negative numbers (`--offset -10`), and the standard `--` positional delimiter.
- **Clean error handling**: It never writes to `stderr` without your permission. It records what went wrong and lets your application decide how to format or log errors.
- **Auto-generated help**: Dynamic column alignment and automated per-flag inspection.

---

## Installation

Download `libargv.h` and include it in your project.

In **exactly one** `.c` translation unit (usually your `main.c`), define `LIBARGV_IMPLEMENTATION` before including the header to emit the implementation code:

```c
#define LIBARGV
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

#define LIBARGV
#include "libargv.h"

int main(int argc, char **argv) {
    // Declare your variables with sensible defaults
    bool verbose = false;
    int verbosity_level = 0;
    int port = 8080;
    double timeout = 15.5;
    const char *config_path = "default.conf";
    const char *input_file = NULL;

    // Initialize the parser
    CliParser parser;
    cli_init(&parser, argv[0]);

    // Bind flags to variables
    cli_bind_bool(&parser, "-v", "--verbose", &verbose, "Turn on debug logs");
    cli_bind_count(&parser, "-l", "--log-level", &verbosity_level, "Repeat for higher verbosity (-l, -ll)");
    cli_bind_int(&parser, "-p", "--port", &port, false, "Port to listen on");
    cli_bind_float(&parser, "-t", "--timeout", &timeout, false, "Connection timeout in seconds");
    cli_bind_str(&parser, "-c", "--config", &config_path, false, "Path to config file");

    // Positional arguments (ordered)
    cli_bind_pos(&parser, "input", &input_file, true, "Input file to process");

    // Parse argv
    CliStatus status = cli_parse(&parser, argc, argv);

    if (status == CLI_HELP_REQUESTED) {
        cli_free(&parser);
        return 0; // Help already printed to stdout
    }

    if (status != CLI_OK) {
        // You choose how to format the error
        fprintf(stderr, "Error: %s\n", cli_get_error(&parser));
        fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
        cli_free(&parser);
        return 1;
    }

    // Use your values directly
    printf("Starting on port %d with config '%s' (input: %s)\n", port, config_path, input_file);

    // Clean up internal parser allocations
    cli_free(&parser);
    return 0;
}
```

Compile it with any C99+ compiler:

```bash
gcc -Wall -Wextra main.c -o main
```

---

## Supported Syntax

| Syntax | Example | Description |
|---|---|---|
| **Standard flags** | `-c config.json`, `--config config.json` | Traditional space-separated options. |
| **Inline values** | `--config=config.json` | Key-value style long flags. |
| **Short clusters** | `-vxl` | Equivalent to passing `-v -x -l` separately. |
| **Attached short values** | `-I/usr/include` | Value immediately following the short option. |
| **Counters** | `-l`, `-ll`, `-lll` | Increments an `int` counter each time it is encountered. |
| **Negative numbers** | `--offset -12`, `-t -0.5` | Won't mistake negative numbers for unrecognized flags. |
| **Double-dash `--`** | `-- -weird-filename.txt` | Treats everything following `--` as a positional argument. |

---

## API Reference

### Setup and Lifecycle

```c
void cli_init(CliParser *p, const char *program_name);
```
Prepares the parser instance and allocates the initial storage for option descriptors. Pass `argv[0]` as `program_name`.

```c
void cli_free(CliParser *p);
```
Frees dynamic memory allocated by the parser. Call this before your application exits.

```c
const char *cli_get_error(const CliParser *p);
```
Returns a pointer to a descriptive, null-terminated error string if `cli_parse` fails.

---

### Binding Options

Each binding helper links an option to a pointer in your code:

```c
void cli_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
```
Flags that take no arguments and flip the boolean target to `true` when present.

```c
void cli_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
```
Flags that increment an integer every time they appear on the command line (handy for verbosity levels like `-v`, `-vv`).

```c
void cli_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *help);
void cli_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *help);
void cli_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *help);
```
Parses integers (via `strtol`), floats (via `strtod`), or direct string pointers (`const char*` pointing into `argv`). If `required` is true, missing the flag triggers `CLI_ERR_MISSING_REQUIRED`.

```c
void cli_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *help);
```
Registers an ordered positional argument. Positional arguments are resolved in the exact order they are registered.

---

### Parsing and Status Codes

```c
CliStatus cli_parse(CliParser *p, int argc, char **argv);
```

Returns one of the following enum values:

- `CLI_OK`: Everything parsed cleanly.
- `CLI_HELP_REQUESTED`: `-h` or `--help` was encountered. Usage text has already been printed to `stdout`.
- `CLI_ERR_UNKNOWN_FLAG`: Passed a flag that was never bound.
- `CLI_ERR_MISSING_VALUE`: A flag expecting a value was left empty.
- `CLI_ERR_INVALID_VALUE`: Failed to convert numeric input (e.g. string passed to an integer flag).
- `CLI_ERR_MISSING_REQUIRED`: An option or positional marked `required` was not found.
- `CLI_ERR_ALLOC`: Internal allocation failed during growth.

---

## Built-in Help System

`libargv` hooks into `-h` and `--help` automatically:

1. **General help**: Running `./mytool --help` lists the usage line, all options aligned dynamically, and registered positional arguments.
2. **Flag inspection**: Running `./mytool --help --port` displays detailed documentation, type requirements, and constraints for that single option.
