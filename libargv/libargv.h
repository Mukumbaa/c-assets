#ifndef LIB_ARGV_H
#define LIB_ARGV_H

// standard types and basic io
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// initial capacity for the dynamic arguments array
#define ARGV_INIT_CAPACITY 8

// supported argument data types
typedef enum {
    ARGV_TYPE_BOOL,
    ARGV_TYPE_COUNT,
    ARGV_TYPE_CHAR,
    ARGV_TYPE_INT,
    ARGV_TYPE_FLOAT,
    ARGV_TYPE_STR,
    ARGV_TYPE_REST,
} CliType;

// return codes for cli parsing operations
typedef enum {
    ARGV_OK = 0,
    ARGV_HELP_REQUESTED,
    ARGV_ERR_UNKNOWN_FLAG,
    ARGV_ERR_MISSING_VALUE,
    ARGV_ERR_INVALID_VALUE,
    ARGV_ERR_MISSING_REQUIRED,
    ARGV_ERR_ALLOC,
} CliStatus;

// metadata and storage pointer for a single cli argument
typedef struct {
    const char *name;
    const char *short_flag;
    const char *long_flag;
    const char *help;
    // allowed values separated by comma or pipe (e.g. "json,xml,csv"), or null
    const char *choices;
    CliType type;
    void *target;
    size_t *count_target;

    bool is_positional;
    bool required;
    bool is_set;
} CliArg;

// top-level parser state
typedef struct {
    const char *program_name;
    CliArg *args;
    size_t count;
    size_t capacity;
    char error[256];
} CliParser;

// parser lifecycle management
extern void argv_init(CliParser *p, const char *program_name);
extern void argv_free(CliParser *p);
extern const char *argv_get_error(const CliParser *p);

// registration helpers for options and flags
extern void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help);
extern void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help);
extern void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help);
extern void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help);
extern void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help);
extern void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help);
// register a positional argument
extern void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help);
extern void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help);

// main parsing entry point and help display
extern CliStatus argv_parse(CliParser *p, int argc, char **argv);
extern void argv_print_help(const CliParser *p);

// header guard end
#endif
