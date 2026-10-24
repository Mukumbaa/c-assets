#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "libargv.h"

// strip directory path so usage lines only show the binary name
static const char *argv_basename(const char *path) {
    if (!path) return "program";
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *base = (p1 > p2) ? p1 : p2;
    return base ? base + 1 : path;
}

// set up initial parser state and allocate the default buffer
extern void argv_init(CliParser *p, const char *program_name) {
    if (p == NULL) return;
    p->program_name = program_name;
    p->count = 0;
    p->capacity = ARGV_INIT_CAPACITY;
    p->error[0] = '\0';
    p->args = (CliArg *)malloc(p->capacity * sizeof(CliArg));
    if (p->args == NULL) p->capacity = 0;
}

// release allocated memory and reset parser fields
extern void argv_free(CliParser *p) {
    if (p == NULL) return;
    if (p->args != NULL) {
        for (size_t i = 0; i < p->count; i++) {
            if (p->args[i].type == ARGV_TYPE_REST && p->args[i].target != NULL) {
                const char ***arr = (const char ***)p->args[i].target;
                if (*arr != NULL) {
                    free(*arr);
                    *arr = NULL;
                }
            }
        }
        free(p->args);
        p->args = NULL;
    }
    p->count = 0;
    p->capacity = 0;
    p->program_name = NULL;
    p->error[0] = '\0';
}

// retrieve the last formatted error message
extern const char *argv_get_error(const CliParser *p) {
    return p ? p->error : "Parser null pointer";
}

// double the argument array capacity when full
static bool argv_ensure_capacity(CliParser *p) {
    if (p->count < p->capacity) return true;
    size_t new_cap = (p->capacity == 0) ? ARGV_INIT_CAPACITY : p->capacity * 2;
    CliArg *new_args = (CliArg *)realloc(p->args, new_cap * sizeof(CliArg));
    if (new_args == NULL) {
        snprintf(p->error, sizeof(p->error), "Memory allocation failed");
        return false;
    }
    p->args = new_args;
    p->capacity = new_cap;
    return true;
}

// helper to append an argument definition into the parser table
static void argv_add_arg_internal(CliParser *p, const char *name, const char *sh, const char *lng, 
                                 CliType type, void *target, size_t *count_target, bool required, bool is_pos, 
                                 const char *choices, const char *help) {
    if (!argv_ensure_capacity(p)) return;

    CliArg *arg = &p->args[p->count++];
    arg->name = name;
    arg->short_flag = sh;
    arg->long_flag = lng;
    arg->type = type;
    arg->target = target;
    arg->count_target = count_target;
    arg->required = required;
    arg->is_positional = is_pos;
    arg->choices = choices;
    arg->is_set = false;
    arg->help = help;
}

// boolean flag toggle (no value expected)
extern void argv_bind_bool(CliParser *p, const char *sh, const char *lng, bool *target, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_BOOL, (void *)target, NULL, false, false, NULL, help);
}

// counter flag that increments with each occurrence (e.g. -v, -vv, -vvv)
extern void argv_bind_count(CliParser *p, const char *sh, const char *lng, int *target, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_COUNT, (void *)target, NULL, false, false, NULL, help);
}

// single character flag
extern void argv_bind_char(CliParser *p, const char *sh, const char *lng, char *target, bool required, const char *choices, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_CHAR, (void *)target, NULL, required, false, choices, help);
}

// string value flag
extern void argv_bind_str(CliParser *p, const char *sh, const char *lng, const char **target, bool required, const char *choices, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_STR, (void *)target, NULL, required, false, choices, help);
}

// integer value flag
extern void argv_bind_int(CliParser *p, const char *sh, const char *lng, int *target, bool required, const char *choices, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_INT, (void *)target, NULL, required, false, choices, help);
}

// floating point value flag
extern void argv_bind_float(CliParser *p, const char *sh, const char *lng, double *target, bool required, const char *choices, const char *help) {
    argv_add_arg_internal(p, NULL, sh, lng, ARGV_TYPE_FLOAT, (void *)target, NULL, required, false, choices, help);
}

// positional argument (without - or --)
extern void argv_bind_pos(CliParser *p, const char *name, const char **target, bool required, const char *choices, const char *help) {
    argv_add_arg_internal(p, name, NULL, NULL, ARGV_TYPE_STR, (void *)target, NULL, required, true, choices, help);
}

extern void argv_bind_rest(CliParser *p, const char *name, const char ***target, size_t *count, bool required, const char *choices, const char *help) {
    if (target != NULL) *target = NULL;
    if (count != NULL) *count = 0;
    argv_add_arg_internal(p, name, NULL, NULL, ARGV_TYPE_REST, (void *)target, count, required, true, choices, help);
}

// validate parsed value against the allowed choices list
static bool argv_check_choice(const CliArg *arg, const char *val_str) {
    if (!arg->choices) return true;

    // compact single-character format without separators (e.g. "abc")
    if (arg->type == ARGV_TYPE_CHAR && strchr(arg->choices, ',') == NULL && strchr(arg->choices, '|') == NULL) {
        return strchr(arg->choices, val_str[0]) != NULL;
    }

    // walk through comma- or pipe-separated tokens
    const char *p = arg->choices;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '|') p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != ',' && *p != '|' && *p != ' ') p++;
        size_t token_len = (size_t)(p - start);

        // string and char comparison against current token
        if (arg->type == ARGV_TYPE_STR || arg->type == ARGV_TYPE_CHAR || arg->type == ARGV_TYPE_REST) {
            if (token_len == strlen(val_str) && strncmp(start, val_str, token_len) == 0) {
                return true;
            }
        // parse and match numeric int values
        } else if (arg->type == ARGV_TYPE_INT) {
            char token_buf[32];
            if (token_len < sizeof(token_buf)) {
                memcpy(token_buf, start, token_len);
                token_buf[token_len] = '\0';
                char *endptr;
                long choice_v = strtol(token_buf, &endptr, 10);
                if (*endptr == '\0' && (int)choice_v == *(int *)(arg->target)) {
                    return true;
                }
            }
        // parse and match floating point values
        } else if (arg->type == ARGV_TYPE_FLOAT) {
            char token_buf[32];
            if (token_len < sizeof(token_buf)) {
                memcpy(token_buf, start, token_len);
                token_buf[token_len] = '\0';
                char *endptr;
                double choice_v = strtod(token_buf, &endptr);
                if (*endptr == '\0' && choice_v == *(double *)(arg->target)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// format help display hint based on argument type or explicit choices
static void argv_format_type_hint(const CliArg *arg, char *out, size_t out_size) {
    if (arg->choices) {
        snprintf(out, out_size, " {%s}", arg->choices);
        return;
    }
    switch (arg->type) {
        case ARGV_TYPE_STR:   snprintf(out, out_size, " <str>"); break;
        case ARGV_TYPE_INT:   snprintf(out, out_size, " <int>"); break;
        case ARGV_TYPE_FLOAT: snprintf(out, out_size, " <num>"); break;
        case ARGV_TYPE_CHAR:  snprintf(out, out_size, " <char>"); break;
        case ARGV_TYPE_REST:  snprintf(out, out_size, "..."); break;
        default: out[0] = '\0'; break;
    }
}

// print detailed help for a specific inspected argument
static void argv_print_single_help(const CliArg *arg) {
    if (arg->is_positional) {
        printf("Positional argument:   <%s>\n", arg->name ? arg->name : "arg");
        printf("Mandatory:             %s\n", arg->required ? "Yes" : "No");
    } else {
        printf("Short flag:            %s\n", arg->short_flag ? arg->short_flag : "None");
        printf("Long flag:             %s\n", arg->long_flag  ? arg->long_flag  : "None");
        printf("Value required:        %s\n", (arg->type != ARGV_TYPE_BOOL && arg->type != ARGV_TYPE_COUNT) ? "Yes" : "No");
        printf("Mandatory:             %s\n", arg->required ? "Yes" : "No");
    }
    if (arg->choices) {
        printf("Allowed choices:       [%s]\n", arg->choices);
    }
    printf("Description:           %s\n", arg->help ? arg->help : "No documentation available.");
}

// generate and print the full usage banner and options listing
extern void argv_print_help(const CliParser *p) {
    if (p == NULL) return;

    printf("Usage: %s [OPTIONS]", argv_basename(p->program_name));

    // show positional parameters in the usage line
    for (size_t i = 0; i < p->count; i++) {
        if (p->args[i].is_positional) {
            char hint[64];
            argv_format_type_hint(&p->args[i], hint, sizeof(hint));
            if (p->args[i].type == ARGV_TYPE_REST) {
                if (p->args[i].required) printf(" <%s...%s>", p->args[i].name, p->args[i].choices ? hint : "");
                else printf(" [%s...%s]", p->args[i].name, p->args[i].choices ? hint : "");
            } else {
                if (p->args[i].required) printf(" <%s%s>", p->args[i].name, p->args[i].choices ? hint : "");
                else printf(" [%s%s]", p->args[i].name, p->args[i].choices ? hint : "");
            }
        }
    }
    printf("\n\n");

    // figure out column padding so descriptions align nicely
    size_t max_len = 20;
    for (size_t i = 0; i < p->count; i++) {
        const CliArg *arg = &p->args[i];
        if (arg->is_positional) continue;

        size_t len = 2;
        if (arg->short_flag && arg->long_flag) len += strlen(arg->short_flag) + 2 + strlen(arg->long_flag);
        else if (arg->short_flag) len += strlen(arg->short_flag);
        else if (arg->long_flag) len += 6 + strlen(arg->long_flag);

        char hint[64];
        argv_format_type_hint(arg, hint, sizeof(hint));
        len += strlen(hint);

        if (len > max_len && len < 42) max_len = len;
    }

    printf("Options:\n");
    printf("  -h, --help%-*s Display this help message\n", (int)(max_len > 10 ? max_len - 12 : 2), "");
    // print each registered option
    for (size_t i = 0; i < p->count; i++) {
        const CliArg *arg = &p->args[i];
        if (arg->is_positional) continue;

        char buf[128] = {0};
        if (arg->short_flag && arg->long_flag) snprintf(buf, sizeof(buf), "  %s, %s", arg->short_flag, arg->long_flag);
        else if (arg->short_flag) snprintf(buf, sizeof(buf), "  %s", arg->short_flag);
        else if (arg->long_flag) snprintf(buf, sizeof(buf), "      %s", arg->long_flag);

        char hint[64];
        argv_format_type_hint(arg, hint, sizeof(hint));
        strncat(buf, hint, sizeof(buf) - strlen(buf) - 1);

        printf("%-*s %s%s\n", (int)max_len, buf, arg->help ? arg->help : "", arg->required ? " [required]" : "");
    }

    // print positional arguments section if any exist
    bool has_pos = false;
    for (size_t i = 0; i < p->count; i++) {
        if (p->args[i].is_positional) { has_pos = true; break; }
    }
    if (has_pos) {
        printf("\nPositional Arguments:\n");
        for (size_t i = 0; i < p->count; i++) {
            const CliArg *arg = &p->args[i];
            if (!arg->is_positional) continue;
            char buf[256] = {0};
            char hint[64];
            argv_format_type_hint(arg, hint, sizeof(hint));
            if (arg->type == ARGV_TYPE_REST) {
                snprintf(buf, sizeof(buf), "  <%s...%s>", arg->name ? arg->name : "arg", arg->choices ? hint : "");
            } else {
                snprintf(buf, sizeof(buf), "  <%s%s>", arg->name ? arg->name : "arg", arg->choices ? hint : "");
            }
            printf("%-*s %s%s\n", (int)max_len, buf, arg->help ? arg->help : "", arg->required ? " [required]" : "");
        }
    }
}

// look up an argument definition by short or long flag string
static CliArg *argv_find_flag(CliParser *p, const char *flag, bool is_short) {
    for (size_t j = 0; j < p->count; j++) {
        if (p->args[j].is_positional) continue;
        if (is_short && p->args[j].short_flag && strcmp(flag, p->args[j].short_flag) == 0) return &p->args[j];
        if (!is_short && p->args[j].long_flag && strcmp(flag, p->args[j].long_flag) == 0) return &p->args[j];
    }
    return NULL;
}

// convert string token into target data type and validate constraints
static CliStatus argv_assign_val(CliParser *p, CliArg *arg, const char *val_str, const char *flag_name) {
    arg->is_set = true;

    if (arg->type == ARGV_TYPE_REST) {
        const char ***arr = (const char ***)arg->target;
        size_t cur_cnt = arg->count_target ? *(arg->count_target) : 0;
        const char **new_arr = (const char **)realloc(*arr, (cur_cnt + 1) * sizeof(const char *));
        if (!new_arr) {
            snprintf(p->error, sizeof(p->error), "Memory allocation failed");
            return ARGV_ERR_ALLOC;
        }
        new_arr[cur_cnt] = val_str;
        *arr = new_arr;
        if (arg->count_target) {
            *(arg->count_target) = cur_cnt + 1;
        }
    } else if (arg->type == ARGV_TYPE_CHAR) {
        if (val_str[0] == '\0' || val_str[1] != '\0') {
            snprintf(p->error, sizeof(p->error), "Invalid character value '%s' for flag '%s' (expected 1 char)", val_str, flag_name);
            return ARGV_ERR_INVALID_VALUE;
        }
        *(char *)(arg->target) = val_str[0];
    } else if (arg->type == ARGV_TYPE_STR) {
        *(const char **)(arg->target) = val_str;
    } else if (arg->type == ARGV_TYPE_INT) {
        char *endptr = NULL;
        errno = 0;
        long v = strtol(val_str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || endptr == val_str) {
            snprintf(p->error, sizeof(p->error), "Invalid integer value '%s' for flag '%s'", val_str, flag_name);
            return ARGV_ERR_INVALID_VALUE;
        }
        *(int *)(arg->target) = (int)v;
    } else if (arg->type == ARGV_TYPE_FLOAT) {
        char *endptr = NULL;
        errno = 0;
        double v = strtod(val_str, &endptr);
        if (errno != 0 || *endptr != '\0' || endptr == val_str) {
            snprintf(p->error, sizeof(p->error), "Invalid floating-point value '%s' for flag '%s'", val_str, flag_name);
            return ARGV_ERR_INVALID_VALUE;
        }
        *(double *)(arg->target) = v;
    }

    // make sure the value is in the allowed choices list
    if (!argv_check_choice(arg, val_str)) {
        snprintf(p->error, sizeof(p->error), "Invalid choice '%s' for %s '%s' (allowed: [%s])",
                 val_str,
                 arg->is_positional ? "positional argument" : "flag",
                 flag_name,
                 arg->choices);
        return ARGV_ERR_INVALID_VALUE;
    }

    return ARGV_OK;
}

// distinguish between a negative numeric value and an unknown flag
static bool argv_is_negative_number(const char *str) {
    if (!str || str[0] != '-' || str[1] == '\0') return false;
    return ((str[1] >= '0' && str[1] <= '9') || str[1] == '.');
}

// main command line parsing routine
extern CliStatus argv_parse(CliParser *p, int argc, char **argv) {
    // sanity check inputs
    if (p == NULL || argv == NULL || argc <= 0) {
        if (p) snprintf(p->error, sizeof(p->error), "Invalid arguments to argv_parse");
        return ARGV_ERR_INVALID_VALUE;
    }
    p->error[0] = '\0';

    // early scan for help flags so we can show help and exit immediately
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            // look for an argument adjacent to the help flag to inspect
            const char *inspect = NULL;
            if (i + 1 < argc && argv[i + 1][0] != '-') inspect = argv[i + 1];
            else if (i > 1 && argv[i - 1][0] != '-') inspect = argv[i - 1];

            if (inspect) {
                CliArg *found = NULL;
                for (size_t j = 0; j < p->count; j++) {
                    if ((p->args[j].short_flag && strcmp(inspect, p->args[j].short_flag) == 0) ||
                        (p->args[j].long_flag  && strcmp(inspect, p->args[j].long_flag) == 0)  ||
                        (p->args[j].name       && strcmp(inspect, p->args[j].name) == 0)) {
                        found = &p->args[j];
                        break;
                    }
                }
                if (found) argv_print_single_help(found);
                else {
                    printf("Argument '%s' not recognized.\n\n", inspect);
                    argv_print_help(p);
                }
            } else {
                argv_print_help(p);
            }
            return ARGV_HELP_REQUESTED;
        }
    }

    bool stop_flags = false;
    int current_pos_index = 0;

    // main loop over all command line tokens
    for (int i = 1; i < argc; i++) {
        const char *token = argv[i];

        // treat everything following a standalone "--" as positional
        if (!stop_flags && strcmp(token, "--") == 0) {
            stop_flags = true;
            continue;
        }

        // handle positional arguments or bare hyphen "-"
        if (stop_flags || token[0] != '-' || strcmp(token, "-") == 0) {
            CliArg *target_pos = NULL;
            int found_pos = 0;
            for (size_t j = 0; j < p->count; j++) {
                if (p->args[j].is_positional) {
                    if (found_pos == current_pos_index) {
                        target_pos = &p->args[j];
                        break;
                    }
                    found_pos++;
                }
            }

            if (target_pos != NULL) {
                CliStatus st = argv_assign_val(p, target_pos, token, target_pos->name ? target_pos->name : "argument");
                if (st != ARGV_OK) return st;
                if (target_pos->type != ARGV_TYPE_REST) {
                    current_pos_index++;
                }
            } else {
                snprintf(p->error, sizeof(p->error), "Unexpected positional argument '%s'", token);
                return ARGV_ERR_UNKNOWN_FLAG;
            }
            continue;
        }

        // handle long flags starting with "--"
        if (token[1] == '-') {
            char flag_name[64] = {0};
            const char *inline_val = strchr(token, '=');

            // split inline value if "--flag=val" syntax is used
            if (inline_val) {
                size_t name_len = (size_t)(inline_val - token);
                if (name_len >= sizeof(flag_name)) name_len = sizeof(flag_name) - 1;
                strncpy(flag_name, token, name_len);
                flag_name[name_len] = '\0';
                // skip past the '=' char
                inline_val++;
            } else {
                strncpy(flag_name, token, sizeof(flag_name) - 1);
            }

            CliArg *matched = argv_find_flag(p, flag_name, false);
            if (!matched) {
                snprintf(p->error, sizeof(p->error), "Unknown flag '%s'", flag_name);
                return ARGV_ERR_UNKNOWN_FLAG;
            }

            if (matched->type == ARGV_TYPE_BOOL) {
                *(bool *)(matched->target) = true;
                matched->is_set = true;
                continue;
            }
            if (matched->type == ARGV_TYPE_COUNT) {
                (*(int *)(matched->target))++;
                matched->is_set = true;
                continue;
            }

            // pull value from next token if not provided inline
            const char *val_str = inline_val;
            if (!val_str) {
                if (i + 1 >= argc || (argv[i + 1][0] == '-' && !argv_is_negative_number(argv[i + 1]))) {
                    snprintf(p->error, sizeof(p->error), "Flag '%s' requires a value", flag_name);
                    return ARGV_ERR_MISSING_VALUE;
                }
                val_str = argv[++i];
            }

            CliStatus st = argv_assign_val(p, matched, val_str, flag_name);
            if (st != ARGV_OK) return st;
            continue;
        }

        // handle short flags, including bundled ones like "-xvf"
        size_t token_len = strlen(token);
        for (size_t c = 1; c < token_len; c++) {
            char single_sh[3] = {'-', token[c], '\0'};
            CliArg *matched = argv_find_flag(p, single_sh, true);

            if (!matched) {
                snprintf(p->error, sizeof(p->error), "Unknown short flag '%s' in '%s'", single_sh, token);
                return ARGV_ERR_UNKNOWN_FLAG;
            }

            if (matched->type == ARGV_TYPE_BOOL) {
                *(bool *)(matched->target) = true;
                matched->is_set = true;
            } else if (matched->type == ARGV_TYPE_COUNT) {
                (*(int *)(matched->target))++;
                matched->is_set = true;
            } else {
                // grab remaining characters in token as value, or consume the next argv entry
                const char *val_str = NULL;
                if (c + 1 < token_len) {
                    val_str = &token[c + 1];
                    c = token_len;
                } else {
                    if (i + 1 >= argc || (argv[i + 1][0] == '-' && !argv_is_negative_number(argv[i + 1]))) {
                        snprintf(p->error, sizeof(p->error), "Flag '%s' requires a value", single_sh);
                        return ARGV_ERR_MISSING_VALUE;
                    }
                    val_str = argv[++i];
                }
                CliStatus st = argv_assign_val(p, matched, val_str, single_sh);
                if (st != ARGV_OK) return st;
                break;
            }
        }
    }

    // verify that all mandatory options and positional arguments were set
    for (size_t i = 0; i < p->count; i++) {
        if (p->args[i].required && !p->args[i].is_set) {
            if (p->args[i].is_positional) {
                snprintf(p->error, sizeof(p->error), "Mandatory positional argument missing: <%s>", p->args[i].name);
            } else {
                snprintf(p->error, sizeof(p->error), "Mandatory flag missing: %s", 
                         p->args[i].long_flag ? p->args[i].long_flag : p->args[i].short_flag);
            }
            return ARGV_ERR_MISSING_REQUIRED;
        }
    }

    return ARGV_OK;
}

