#ifndef LIB_ARGV_H
#define LIB_ARGV_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#define ARGV_INIT_CAPACITY 8

typedef enum {
  ARGV_TYPE_BOOL,
  ARGV_TYPE_COUNT,
  ARGV_TYPE_CHAR,
  ARGV_TYPE_INT,
  ARGV_TYPE_FLOAT,
  ARGV_TYPE_STR,
  ARGV_TYPE_REST,
} CliType;

typedef enum {
  ARGV_OK = 0,
  ARGV_HELP_REQUESTED,
  ARGV_VERSION_REQUESTED,
  ARGV_ERR_UNKNOWN_FLAG,
  ARGV_ERR_MISSING_VALUE,
  ARGV_ERR_INVALID_VALUE,
  ARGV_ERR_MISSING_REQUIRED,
  ARGV_ERR_MUTUAL_EXCLUSION,
  ARGV_ERR_DEPENDENCY,
  ARGV_ERR_ALLOC,
} CliStatus;

typedef enum {
  ARGV_RULE_EXCLUSIVE,
  ARGV_RULE_DEPENDENCY
} CliRuleType;

typedef struct {
  CliRuleType type;
  const char *target;
  const char *requires;
} CliRule;

typedef struct {
  const char *choice;
  const char *help;
} CliChoiceHelp;

typedef struct {
  const char *name;
  char short_flag[16];
  char long_flag[64];
  const char *help;
  const char *choices;
  const char *env_var;
  const char *default_val;
  CliChoiceHelp *choice_helps;
  size_t choice_help_count;
  size_t choice_help_capacity;
  CliType type;
  void *target;
  size_t *count_target;

  bool is_positional;
  bool required;
  bool is_set;
} CliArg;

typedef struct {
  const char *program_name; 
  const char *version;
  char version_short[16];
  const char *version_extra;
  char env_prefix[64];
  FILE *output;
  CliArg *args;
  size_t count;
  size_t capacity;

  CliRule *rules;
  size_t rule_count;
  size_t rule_capacity;

  char error[256];
  bool init_failed;
} CliParser;

// Redirects help and version output to a custom stream (defaults to stdout)
extern void argv_set_output(CliParser *p, FILE *out);

// Sets a global prefix for environment variable lookups on long flags
extern void argv_set_env_prefix(CliParser *p, const char *prefix);

// Initializes an argument parser instance with the given program name
extern void argv_init(CliParser *p, const char *program_name);

// Frees dynamic resources allocated by the parser instance
extern void argv_free(CliParser *p);

// Returns the most recent error message recorded by the parser
extern const char *argv_get_error(const CliParser *p);

// Binds a boolean flag with support for --no- negation
extern void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);

// Binds a flag that increments an integer counter each time it appears
extern void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);

// Binds a single-character flag argument
extern void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);

// Binds a string flag argument
extern void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);

// Binds an integer flag argument
extern void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);

// Binds a double-precision floating-point flag argument
extern void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);

// Binds a positional string argument
extern void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);

// Binds an array to collect all remaining positional arguments and stores the count
extern void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);

// Maps a specific environment variable name to a flag or positional argument
extern void argv_set_env(CliParser *p, const char *flag_or_name, const char *env_var);

// Sets the default value for an argument, assigns it to target, and displays it in help
extern void argv_set_default(CliParser *p, const char *flag_or_name, const char *default_val);

// Attaches an explanatory help description to an allowed choice value
extern bool argv_set_choice_help(CliParser *p, const char *flag_or_name, const char *choice, const char *help);

// Declares a group of flags (delimited by commas or pipes) that cannot be used together
extern void argv_add_exclusive(CliParser *p, const char *flags_group);

// Enforces that flag requires one or more other flags to be present when supplied
extern void argv_add_dependency(CliParser *p, const char *flag, const char *requires_flags);

// Parses command-line tokens, evaluates rules, inspects env vars, and sets targets
extern CliStatus argv_parse(CliParser *p, int argc, const char *const *argv);

// Prints the complete usage manual and available options to the configured stream
extern void argv_print_help(const CliParser *p);

// Configures version information and enables the --version flag and optional short alia
extern void argv_set_version(CliParser *p, const char *version, const char *sh, const char *extra_info);

#endif
