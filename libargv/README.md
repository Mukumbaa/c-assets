# libargv

A single-header C99 command-line argument parser with direct variable binding, validation rules, and zero dependencies.

---

## Features

- **Single-header**: Drop `libargv.h` into your project and define `LIBARGV_IMPLEMENTATION` in one source file.
- **Direct variable binding**: Binds parsed arguments directly to `bool`, `char`, `int`, `double`, and `const char*` pointers.
- **Explicit defaults**: Set initial values and help-text defaults in one call via `argv_set_default`.
- **Explicit presence check**: Query whether an argument was supplied via CLI or environment variable using `argv_is_set`.
- **Validation & constraints**: Whitelist choices (`"json,csv,yaml"`), mutual exclusion groups (`argv_add_exclusive`), at-least-one groups (`argv_add_any_of`), cross-flag dependencies (`argv_add_dependency`), and duplicate flag detection.
- **POSIX & GNU syntax**: Short-flag clustering (`-xvf`), inline values (`--port=8080`), attached values (`-p8080`, `-p=8080`), negative numbers (`--offset -12`), and stdin/stdout markers (`-o -`).
- **Standard `--` delimiter**: Treats all tokens following `--` as positional arguments.
- **Variadic rest arguments**: Collect trailing arguments into a dynamically allocated array.
- **Environment variable fallback**: Reads unset options from custom or prefixed environment variables (`APP_PORT`).
- **Redirectable output**: Send help and version output to any `FILE*` stream for automated testing.
- **Repeatable flags**: Collect repeated option occurrences into dynamic arrays via `argv_bind_str_list`, `argv_bind_int_list`, and `argv_bind_float_list`.
- **Clean subcommands**: Stop flag/help parsing at the first positional argument via `argv_set_stop_at_positional` and forward `argv_remaining_args` to sub-parsers.
- **Rich help customization**: Program description (`argv_set_description`), epilog (`argv_set_epilog`), grouped option sections (`argv_set_help_group`), custom placeholders (`argv_set_placeholder`), configurable/auto `COLUMNS` wrap width (`argv_set_help_width`), and hidden flags (`argv_set_hidden`).
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
    bool dry_run = false;
    bool compress = false;
    int verbosity = 0;
    int port = 0;
    int offset = 0;
    char mode = 'r';
    const char *format = NULL;
    const char *output = NULL;
    bool encrypt = false;
    bool plain = false;
    const char *key = NULL;

    const char *input_file = NULL;
    const char **extra_files = NULL;
    size_t extra_count = 0;

    CliParser p;
    argv_init(&p, argv[0]);

    argv_set_version(&p, "1.2.0", "V", "Build 2026.1");
    argv_set_description(&p, "Fast data transformation and encryption utility.");
    argv_set_epilog(&p, "Examples:\n  datax -f json -o out.json input.csv");
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

    // Custom placeholders & help groups
    argv_set_placeholder(&p, "output", "path");
    argv_set_help_group(&p, "encrypt", "Security Options");
    argv_set_help_group(&p, "plain", "Security Options");
    argv_set_help_group(&p, "key", "Security Options");

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

    // Positional arguments (scalar positionals must be bound before rest arguments)
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

    printf("Input: %s, Port: %d (explicit: %s), Mode: %c, Output: %s\n",
        input_file, port, argv_is_set(&p, "port") ? "yes" : "no", mode, output);

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
| No-dash binding | `argv_bind_bool(&p, "v", "verbose", ...)` | Accepts options with or without leading hyphens. |
| Standard flags | `-p 8080`, `--port 8080` | Space-separated flag values. |
| Inline values | `--port=8080` | Key-value flag syntax. |
| Boolean negation | `--no-compress`, `--compress=false` | Disables a boolean switch. |
| Short clusters | `-xvf` | Expands to `-x -v -f`. |
| Attached values | `-p8080`, `-p=8080`, `-I/usr/include` | Value attached directly to the short flag. |
| Counter flags | `-v`, `-vv`, `-vvv` | Increments an `int` counter. |
| Negative numbers | `--offset -12` | Negative numeric literals are not parsed as flags. |
| Stdin/stdout flag | `-o -` | Preserves `-` as a literal value. |
| Delimiter | `-- -file.txt` | Treats everything after `--` as positional arguments. |

---

## Subcommands

Multi-command CLIs can enable `argv_set_stop_at_positional` on the root parser so global flags are parsed up to the subcommand name, and all trailing tokens (including `--help`) are forwarded intact via `argv_remaining_args`:

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define LIBARGV_IMPLEMENTATION
#include "libargv.h"

static void run_commit(bool verbose, int argc, const char *const *argv);

int main(int argc, char **argv) {
    bool verbose = false;
    const char *cmd = NULL;

    CliParser root;
    argv_init(&root, "git");
    argv_set_stop_at_positional(&root, true);
    argv_bind_bool(&root, "v", "verbose", &verbose, "Enable verbose output");
    argv_bind_pos(&root, "command", &cmd, true, "commit,clone", "Subcommand to run");
    argv_set_choice_help(&root, "command", "commit", "Record repository changes");
    argv_set_choice_help(&root, "command", "clone", "Clone remote repository");

    CliStatus st = argv_parse(&root, argc, (const char *const *)argv);
    if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
        argv_free(&root);
        return 0;
    }
    if (st != ARGV_OK) {
        fprintf(stderr, "git: %s\n", argv_get_error(&root));
        argv_free(&root);
        return 1;
    }

    int rem_argc = 0;
    const char *const *rem_argv = argv_remaining_args(&root, &rem_argc);

    if (strcmp(cmd, "commit") == 0) {
        if (rem_argc > 0) {
            run_commit(verbose, rem_argc + 1, rem_argv - 1);
        } else {
            const char *const empty_argv[] = {cmd};
            run_commit(verbose, 1, empty_argv);
        }
    }

    argv_free(&root);
    return 0;
}

static void run_commit(bool verbose, int argc, const char *const *argv) {
    const char *message = NULL;
    CliParser p;
    argv_init(&p, "git commit");
    argv_bind_str(&p, "m", "message", &message, true, NULL, "Commit message");

    CliStatus st = argv_parse(&p, argc, argv);
    if (st == ARGV_HELP_REQUESTED) {
        argv_free(&p);
        exit(0);
    }
    if (st != ARGV_OK) {
        fprintf(stderr, "commit: %s\n", argv_get_error(&p));
        argv_free(&p);
        exit(1);
    }
    printf("[verbose=%d] Committed: %s\n", verbose, message);
    argv_free(&p);
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
void argv_set_description(CliParser *p, const char *description);
void argv_set_epilog(CliParser *p, const char *epilog);
void argv_set_help_width(CliParser *p, size_t width);
void argv_set_env_prefix(CliParser *p, const char *prefix);
void argv_set_stop_at_positional(CliParser *p, bool stop);
```

### Option Binding

```c
void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);
void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);

// Note: Rest arguments must be bound after all scalar positional arguments
void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);
void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);
void argv_bind_str_list(CliParser *p, const char *sh, const char *lng, const char ***target, size_t *count, bool required, const char *choices, const char *help);
void argv_bind_int_list(CliParser *p, const char *sh, const char *lng, int **target, size_t *count, bool required, const char *choices, const char *help);
void argv_bind_float_list(CliParser *p, const char *sh, const char *lng, double **target, size_t *count, bool required, const char *choices, const char *help);
```

### Defaults, Rules & Documentation

```c
void argv_set_default(CliParser *p, const char *flag_or_name, const char *default_val);
void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var);
bool argv_set_choice_help(CliParser *p, const char *flag_or_name, const char *choice, const char *help);
void argv_set_placeholder(CliParser *p, const char *flag_or_name, const char *placeholder);
void argv_set_help_group(CliParser *p, const char *flag_or_name, const char *group);
void argv_set_hidden(CliParser *p, const char *flag_or_name);
void argv_add_exclusive(CliParser *p, const char *flags_group);
void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags);
void argv_add_any_of(CliParser *p, const char *flags_group);
```

### Execution

```c
CliStatus argv_parse(CliParser *p, int argc, const char *const *argv);
bool argv_is_set(const CliParser *p, const char *flag_or_name);
const char *const *argv_remaining_args(const CliParser *p, int *out_argc);
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

## Full example
```c
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "libargv.h"

typedef struct {
  bool quiet;
  int verbosity;
  const char *context;
} GlobalOpts;

static int cmd_deploy(const GlobalOpts *g, int argc, const char *const *argv);
static int cmd_logs(const GlobalOpts *g, int argc, const char *const *argv);
static int cmd_config(const GlobalOpts *g, int argc, const char *const *argv);

int main(int argc, char **argv) {
  GlobalOpts global = {false, 0, NULL};
  const char *command = NULL;

  CliParser root;
  argv_init(&root, argv[0]);
  argv_set_output(&root, stdout);
  argv_set_version(&root, "1.4.0", "V", "Build 2026.11 (C99)");
  argv_set_description(&root, "Service deployment and operations CLI.");
  argv_set_epilog(&root, "Run 'svcctl <command> --help' for command-specific options.");
  argv_set_help_width(&root, 60);
  argv_set_env_prefix(&root, "SVCCTL");

  /* Stop parsing global flags once the subcommand positional is reached */
  argv_set_stop_at_positional(&root, true);

  /* Global options */
  argv_bind_bool(&root, "q", "quiet", &global.quiet, "Suppress informational output");
  argv_bind_count(&root, "v", "verbose", &global.verbosity, "Increase log verbosity (-v, -vv)");
  argv_bind_str(&root, "C", "context", &global.context, false, NULL, "Target cluster context");
  argv_set_placeholder(&root, "context", "name");
  argv_set_default(&root, "context", "default");

  /* Subcommand selector */
  argv_bind_pos(&root, "command", &command, true, "deploy,logs,config", "Subcommand to execute");
  argv_set_choice_help(&root, "command", "deploy", "Deploy a service and expose ports");
  argv_set_choice_help(&root, "command", "logs", "Inspect or stream service logs");
  argv_set_choice_help(&root, "command", "config", "Read or update CLI configuration");

  CliStatus st = argv_parse(&root, argc, (const char *const *)argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&root);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "svcctl: %s\n", argv_get_error(&root));
    if (st == ARGV_ERR_MISSING_REQUIRED) {
      fprintf(stderr, "\n");
      argv_set_output(&root, stderr);
      argv_print_help(&root);
    }
    argv_free(&root);
    return 1;
  }

  /* Prepare argv slice for the selected subcommand (argv[0] = subcommand name) */
  int rem_argc = 0;
  const char *const *rem_argv = argv_remaining_args(&root, &rem_argc);
  const char *const empty_argv[] = {command};
  int sub_argc = (rem_argc > 0) ? (rem_argc + 1) : 1;
  const char *const *sub_argv = (rem_argc > 0) ? (rem_argv - 1) : empty_argv;

  int exit_code = 0;
  if (strcmp(command, "deploy") == 0) {
    exit_code = cmd_deploy(&global, sub_argc, sub_argv);
  } else if (strcmp(command, "logs") == 0) {
    exit_code = cmd_logs(&global, sub_argc, sub_argv);
  } else if (strcmp(command, "config") == 0) {
    exit_code = cmd_config(&global, sub_argc, sub_argv);
  }

  /* Free root only after subcommands finish using command / rem_argv */
  argv_free(&root);
  return exit_code;
}

static int cmd_deploy(const GlobalOpts *g, int argc, const char *const *argv) {
  bool dry_run = false;
  bool health_check = true; /* Default true -> displays --[no-]health-check in help */
  char mode = 'r';
  int port = 0;
  int offset = 0;
  double traffic = 0.0;
  const char *strategy = NULL;
  const char *output_path = NULL;

  bool encrypt = false;
  bool plain = false;
  const char *key_file = NULL;
  bool debug_hooks = false;

  const char **env_vars = NULL;
  size_t env_count = 0;
  int *exposed_ports = NULL;
  size_t exposed_count = 0;
  double *weights = NULL;
  size_t weight_count = 0;

  const char *service = NULL;
  const char **artifacts = NULL;
  size_t artifact_count = 0;

  CliParser p;
  argv_init(&p, "svcctl deploy");
  argv_set_output(&p, stdout);
  argv_set_version(&p, "1.4.0", "V", "Subcommand: deploy");
  argv_set_description(&p, "Deploy a service bundle to the selected cluster context.");
  argv_set_epilog(&p,
      "Examples:\n"
      "  svcctl -vv deploy --plain --no-health-check -p 9090 -E ENV=prod -P 80 -P 443 api ./dist.tar.gz");
  argv_set_help_width(&p, 0); /* 0 = auto-detect from COLUMNS or fallback to 50 */
  argv_set_env_prefix(&p, "SVCCTL_DEPLOY");

  /* Scalar options */
  argv_bind_bool(&p, "d", "dry-run", &dry_run, "Simulate deployment without applying changes");
  argv_bind_bool(&p, "H", "health-check", &health_check, "Wait for readiness probes after rollout");
  argv_bind_char(&p, "m", "mode", &mode, false, "r,b,c", "Rollout mode");
  argv_bind_int(&p, "p", "port", &port, false, NULL, "Primary container listen port");
  argv_bind_int(&p, "O", "offset", &offset, false, NULL, "Canary skew offset (allows negative values)");
  argv_bind_float(&p, "t", "traffic", &traffic, false, NULL, "Initial traffic ratio (0.0 - 1.0)");
  argv_bind_str(&p, "s", "strategy", &strategy, false, "fast,safe,canary", "Deployment strategy");
  argv_bind_str(&p, "o", "output", &output_path, false, NULL, "Manifest output path ('-' for stdout)");

  /* Grouped security options & hidden diagnostic flag */
  argv_bind_bool(&p, "e", "encrypt", &encrypt, "Encrypt bundle in transit");
  argv_bind_bool(&p, NULL, "plain", &plain, "Disable transport encryption");
  argv_bind_str(&p, "k", "key-file", &key_file, false, NULL, "TLS private key path");
  argv_bind_bool(&p, "D", "debug-hooks", &debug_hooks, "Enable internal lifecycle hook tracing");

  /* Repeatable list options */
  argv_bind_str_list(&p, "E", "env", &env_vars, &env_count, false, NULL, "Environment variables (KEY=VAL)");
  argv_bind_int_list(&p, "P", "expose", &exposed_ports, &exposed_count, false, NULL, "Additional ports to expose");
  argv_bind_float_list(&p, "w", "weight", &weights, &weight_count, false, NULL, "Traffic weights per replica pool");

  /* Help customization & environment overrides */
  argv_set_placeholder(&p, "output", "path");
  argv_set_placeholder(&p, "key-file", "pem");
  argv_set_placeholder(&p, "env", "K=V");
  argv_set_help_group(&p, "encrypt", "Security Options");
  argv_set_help_group(&p, "plain", "Security Options");
  argv_set_help_group(&p, "key-file", "Security Options");
  argv_set_env(&p, "key-file", "DEPLOY_TLS_KEY");
  argv_set_hidden(&p, "debug-hooks");

  /* Explicit defaults */
  argv_set_default(&p, "dry-run", "false");
  argv_set_default(&p, "health-check", "true");
  argv_set_default(&p, "mode", "r");
  argv_set_default(&p, "port", "8080");
  argv_set_default(&p, "offset", "0");
  argv_set_default(&p, "traffic", "1.0");
  argv_set_default(&p, "strategy", "safe");
  argv_set_default(&p, "output", "-");

  /* Choice documentation */
  argv_set_choice_help(&p, "mode", "r", "Rolling update");
  argv_set_choice_help(&p, "mode", "b", "Blue-green swap");
  argv_set_choice_help(&p, "mode", "c", "Canary progressive");
  argv_set_choice_help(&p, "strategy", "fast", "Minimize rollout time");
  argv_set_choice_help(&p, "strategy", "safe", "Verify health checks between batches");
  argv_set_choice_help(&p, "strategy", "canary", "Shift traffic gradually");

  /* Cross-flag validation rules:
   * Combining exclusive + any-of enforces "exactly one" transport security flag */
  argv_add_exclusive(&p, "encrypt, plain");
  argv_add_any_of(&p, "encrypt, plain");
  argv_add_dependency(&p, "key-file", "encrypt");

  /* Positionals: bind all scalar positionals before any rest argument */
  argv_bind_pos(&p, "service", &service, true, NULL, "Target service name");
  argv_bind_rest(&p, "artifacts", &artifacts, &artifact_count, false, NULL, "Optional bundle files");
  argv_set_placeholder(&p, "artifacts", "file");

  CliStatus st = argv_parse(&p, argc, argv);
  switch (st) {
    case ARGV_OK:
      break;
    case ARGV_HELP_REQUESTED:
    case ARGV_VERSION_REQUESTED:
      argv_free(&p);
      return 0;
    case ARGV_ERR_UNKNOWN_FLAG:
      fprintf(stderr, "deploy: unrecognized option: %s\n", argv_get_error(&p));
      argv_free(&p);
      return 2;
    case ARGV_ERR_MISSING_VALUE:
    case ARGV_ERR_INVALID_VALUE:
    case ARGV_ERR_MISSING_REQUIRED:
    case ARGV_ERR_MUTUAL_EXCLUSION:
    case ARGV_ERR_DEPENDENCY:
    case ARGV_ERR_ALLOC:
    default:
      fprintf(stderr, "deploy: %s\n", argv_get_error(&p));
      argv_free(&p);
      return 1;
  }

  /* Best practice: use argv_is_set to distinguish explicit CLI/env values from defaults */
  bool port_explicit = argv_is_set(&p, "port");

  /* Use all parsed values (including env-backed strings and lists) before argv_free */
  if (!g->quiet) {
    printf("[context=%s verbosity=%d] Deploying service '%s'\n", g->context, g->verbosity, service);
    printf("  dry_run=%s health_check=%s mode=%c port=%d (%s) offset=%d traffic=%.2f strategy=%s output=%s\n",
        dry_run ? "true" : "false", health_check ? "true" : "false",
        mode, port, port_explicit ? "explicit" : "default", offset, traffic, strategy, output_path);
    printf("  encrypt=%s plain=%s key_file=%s debug_hooks=%s\n",
        encrypt ? "true" : "false", plain ? "true" : "false",
        key_file ? key_file : "(none)", debug_hooks ? "true" : "false");

    printf("  env (%zu):", env_count);
    for (size_t i = 0; i < env_count; i++) printf(" %s", env_vars[i]);
    printf("\n");

    printf("  exposed_ports (%zu):", exposed_count);
    for (size_t i = 0; i < exposed_count; i++) printf(" %d", exposed_ports[i]);
    printf("\n");

    printf("  weights (%zu):", weight_count);
    for (size_t i = 0; i < weight_count; i++) printf(" %.2f", weights[i]);
    printf("\n");

    printf("  artifacts (%zu):", artifact_count);
    for (size_t i = 0; i < artifact_count; i++) printf(" %s", artifacts[i]);
    printf("\n");
  }

  argv_free(&p);
  return 0;
}

static int cmd_logs(const GlobalOpts *g, int argc, const char *const *argv) {
  bool follow = false;
  int lines = 0;
  const char *level = NULL;
  const char *since = NULL;
  const char **filters = NULL;
  size_t filter_count = 0;
  const char *service = NULL;

  CliParser p;
  argv_init(&p, "svcctl logs");
  argv_set_description(&p, "Fetch or stream logs from a running service.");
  argv_set_env_prefix(&p, "SVCCTL_LOGS");

  argv_bind_bool(&p, "f", "follow", &follow, "Follow log output");
  argv_bind_int(&p, "n", "lines", &lines, false, NULL, "Number of recent lines to show");
  argv_bind_str(&p, "l", "level", &level, false, "debug,info,warn,error", "Minimum log severity");
  argv_bind_str(&p, "s", "since", &since, false, NULL, "Show logs newer than duration");
  argv_bind_str_list(&p, "F", "filter", &filters, &filter_count, false, NULL, "Include only matching tags");
  argv_bind_pos(&p, "service", &service, true, NULL, "Target service name");

  argv_set_placeholder(&p, "since", "duration");
  argv_set_placeholder(&p, "filter", "tag");
  argv_set_default(&p, "lines", "100");
  argv_set_default(&p, "level", "info");
  argv_set_default(&p, "since", "15m");

  CliStatus st = argv_parse(&p, argc, argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&p);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "logs: %s\n", argv_get_error(&p));
    argv_free(&p);
    return 1;
  }

  if (!g->quiet) {
    printf("[context=%s] Streaming logs for '%s' (follow=%s lines=%d (%s) level=%s since=%s filters=%zu)\n",
        g->context, service, follow ? "true" : "false",
        lines, argv_is_set(&p, "lines") ? "explicit" : "default",
        level, since, filter_count);
    for (size_t i = 0; i < filter_count; i++) {
      printf("  filter[%zu]=%s\n", i, filters[i]);
    }
  }

  argv_free(&p);
  return 0;
}

static int cmd_config(const GlobalOpts *g, int argc, const char *const *argv) {
  bool global_scope = false;
  const char *format = NULL;
  const char *action = NULL;
  const char **rest_args = NULL;
  size_t rest_count = 0;

  CliParser p;
  argv_init(&p, "svcctl config");
  argv_set_description(&p, "Inspect or modify CLI configuration profiles.");

  argv_bind_bool(&p, "g", "global", &global_scope, "Operate on global configuration");
  argv_bind_str(&p, "f", "format", &format, false, "text,json", "Output serialization format");
  argv_bind_pos(&p, "action", &action, true, "get,set,list", "Configuration action");
  argv_bind_rest(&p, "args", &rest_args, &rest_count, false, NULL, "Action arguments (<key> or <key> <value>)");

  argv_set_default(&p, "format", "text");
  argv_set_choice_help(&p, "action", "get", "Read a configuration key");
  argv_set_choice_help(&p, "action", "set", "Write a configuration key/value pair");
  argv_set_choice_help(&p, "action", "list", "List all configuration entries");

  CliStatus st = argv_parse(&p, argc, argv);
  if (st == ARGV_HELP_REQUESTED || st == ARGV_VERSION_REQUESTED) {
    argv_free(&p);
    return 0;
  }
  if (st != ARGV_OK) {
    fprintf(stderr, "config: %s\n", argv_get_error(&p));
    argv_free(&p);
    return 1;
  }

  /* Post-parse domain validation */
  if (strcmp(action, "get") == 0 && rest_count != 1) {
    fprintf(stderr, "config: 'get' requires exactly 1 argument (<key>)\n");
    argv_free(&p);
    return 1;
  }
  if (strcmp(action, "set") == 0 && rest_count != 2) {
    fprintf(stderr, "config: 'set' requires exactly 2 arguments (<key> <value>)\n");
    argv_free(&p);
    return 1;
  }

  if (!g->quiet) {
    printf("[context=%s] config action=%s global=%s format=%s",
        g->context, action, global_scope ? "true" : "false", format);
    if (rest_count == 1) printf(" key=%s", rest_args[0]);
    if (rest_count == 2) printf(" key=%s value=%s", rest_args[0], rest_args[1]);
    printf("\n");
  }

  argv_free(&p);
  return 0;
}
```
