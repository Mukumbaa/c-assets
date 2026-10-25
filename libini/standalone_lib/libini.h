/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBINI_IMPLEMENTATION before including this file:
 * #define LIBINI_IMPLEMENTATION
 * #include "libini.h"
 */

#ifndef LIBINI_DEF
#ifdef __cplusplus
#define LIBINI_DEF extern "C"
#else
#define LIBINI_DEF extern
#endif
#endif

#ifndef LIB_INI_H
#define LIB_INI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define INI_INIT_CAPACITY 16

typedef enum {
    INI_TYPE_BOOL,
    INI_TYPE_INT,
    INI_TYPE_FLOAT,
    INI_TYPE_CHAR,
    INI_TYPE_STR,
} IniType;

typedef enum {
    INI_OK = 0,
    INI_ERR_FILE_IO,
    INI_ERR_SYNTAX,
    INI_ERR_UNKNOWN_KEY,
    INI_ERR_INVALID_VALUE,
    INI_ERR_MISSING_REQUIRED,
    INI_ERR_ALLOC,
} IniStatus;

typedef struct {
    const char *section;
    const char *key;
    IniType type;
    void *target;
    size_t max_size;
    bool required;
    const char *choices;
    bool is_set;
} IniOption;

typedef struct {
    IniOption *options;
    size_t count;
    size_t capacity;
    char error[256];
} IniParser;

void ini_init(IniParser *p);
void ini_free(IniParser *p);
const char *ini_get_error(const IniParser *p);

void ini_bind_bool(IniParser *p, const char *section, const char *key, bool *target, bool required);
void ini_bind_int(IniParser *p, const char *section, const char *key, int *target, bool required, const char *choices);
void ini_bind_float(IniParser *p, const char *section, const char *key, double *target, bool required, const char *choices);
void ini_bind_char(IniParser *p, const char *section, const char *key, char *target, bool required, const char *choices);
void ini_bind_str(IniParser *p, const char *section, const char *key, char *target, size_t max_size, bool required, const char *choices);

IniStatus ini_parse_file(IniParser *p, const char *filename);
IniStatus ini_parse_string(IniParser *p, const char *content);

#endif

#ifdef LIBINI_IMPLEMENTATION
#ifndef LIBINI_IMPLEMENTED
#define LIBINI_IMPLEMENTED

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

void ini_init(IniParser *p) {
    if (p == NULL) return;
    p->count = 0;
    p->capacity = INI_INIT_CAPACITY;
    p->error[0] = '\0';
    p->options = (IniOption *)malloc(p->capacity * sizeof(IniOption));
    if (p->options == NULL) p->capacity = 0;
}

void ini_free(IniParser *p) {
    if (p == NULL) return;
    if (p->options != NULL) {
        free(p->options);
        p->options = NULL;
    }
    p->count = 0;
    p->capacity = 0;
    p->error[0] = '\0';
}

const char *ini_get_error(const IniParser *p) {
    return p ? p->error : "Parser null pointer";
}

static bool ini_ensure_capacity(IniParser *p) {
    if (p->count < p->capacity) return true;
    size_t new_cap = (p->capacity == 0) ? INI_INIT_CAPACITY : p->capacity * 2;
    IniOption *new_opts = (IniOption *)realloc(p->options, new_cap * sizeof(IniOption));
    if (new_opts == NULL) {
        snprintf(p->error, sizeof(p->error), "Memory allocation failed");
        return false;
    }
    p->options = new_opts;
    p->capacity = new_cap;
    return true;
}

static void ini_add_option_internal(IniParser *p, const char *section, const char *key,
                                    IniType type, void *target, size_t max_size,
                                    bool required, const char *choices) {
    if (!ini_ensure_capacity(p)) return;

    IniOption *opt = &p->options[p->count++];
    opt->section = section;
    opt->key = key;
    opt->type = type;
    opt->target = target;
    opt->max_size = max_size;
    opt->required = required;
    opt->choices = choices;
    opt->is_set = false;
}

void ini_bind_bool(IniParser *p, const char *section, const char *key, bool *target, bool required) {
    ini_add_option_internal(p, section, key, INI_TYPE_BOOL, (void *)target, 0, required, NULL);
}

void ini_bind_int(IniParser *p, const char *section, const char *key, int *target, bool required, const char *choices) {
    ini_add_option_internal(p, section, key, INI_TYPE_INT, (void *)target, 0, required, choices);
}

void ini_bind_float(IniParser *p, const char *section, const char *key, double *target, bool required, const char *choices) {
    ini_add_option_internal(p, section, key, INI_TYPE_FLOAT, (void *)target, 0, required, choices);
}

void ini_bind_char(IniParser *p, const char *section, const char *key, char *target, bool required, const char *choices) {
    ini_add_option_internal(p, section, key, INI_TYPE_CHAR, (void *)target, 0, required, choices);
}

void ini_bind_str(IniParser *p, const char *section, const char *key, char *target, size_t max_size, bool required, const char *choices) {
    ini_add_option_internal(p, section, key, INI_TYPE_STR, (void *)target, max_size, required, choices);
}

static bool ini_strcasecmp(const char *s1, const char *s2) {
    if (s1 == s2) return true;
    if (!s1 || !s2) return false;
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) return false;
        s1++;
        s2++;
    }
    return (*s1 == '\0' && *s2 == '\0');
}

static char *ini_trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static IniOption *ini_find_option(IniParser *p, const char *section, const char *key) {
    for (size_t i = 0; i < p->count; i++) {
        IniOption *opt = &p->options[i];

        bool sec_matches = false;
        if ((!opt->section || opt->section[0] == '\0') && (!section || section[0] == '\0')) {
            sec_matches = true;
        } else if (opt->section && section) {
            sec_matches = ini_strcasecmp(opt->section, section);
        }

        if (sec_matches && ini_strcasecmp(opt->key, key)) {
            return opt;
        }
    }
    return NULL;
}

static bool ini_check_choice(const IniOption *opt, const char *val_str) {
    if (!opt->choices) return true;

    if (opt->type == INI_TYPE_CHAR && strchr(opt->choices, ',') == NULL && strchr(opt->choices, '|') == NULL) {
        return strchr(opt->choices, val_str[0]) != NULL;
    }

    const char *p = opt->choices;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '|') p++;
        if (!*p) break;

        const char *start = p;
        while (*p && *p != ',' && *p != '|' && *p != ' ') p++;
        size_t token_len = (size_t)(p - start);

        if (opt->type == INI_TYPE_STR || opt->type == INI_TYPE_CHAR) {
            if (token_len == strlen(val_str) && strncmp(start, val_str, token_len) == 0) {
                return true;
            }
        } else if (opt->type == INI_TYPE_INT) {
            char token_buf[32];
            if (token_len < sizeof(token_buf)) {
                memcpy(token_buf, start, token_len);
                token_buf[token_len] = '\0';
                char *endptr;
                long choice_v = strtol(token_buf, &endptr, 10);
                if (*endptr == '\0' && (int)choice_v == *(int *)(opt->target)) {
                    return true;
                }
            }
        } else if (opt->type == INI_TYPE_FLOAT) {
            char token_buf[32];
            if (token_len < sizeof(token_buf)) {
                memcpy(token_buf, start, token_len);
                token_buf[token_len] = '\0';
                char *endptr;
                double choice_v = strtod(token_buf, &endptr);
                if (*endptr == '\0' && choice_v == *(double *)(opt->target)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static IniStatus ini_assign_val(IniParser *p, IniOption *opt, const char *val_str, int line_num) {
    opt->is_set = true;

    if (opt->type == INI_TYPE_BOOL) {
        if (ini_strcasecmp(val_str, "true") || ini_strcasecmp(val_str, "1") ||
            ini_strcasecmp(val_str, "yes")  || ini_strcasecmp(val_str, "on")) {
            *(bool *)(opt->target) = true;
        } else if (ini_strcasecmp(val_str, "false") || ini_strcasecmp(val_str, "0") ||
                   ini_strcasecmp(val_str, "no")    || ini_strcasecmp(val_str, "off")) {
            *(bool *)(opt->target) = false;
        } else {
            snprintf(p->error, sizeof(p->error), "line %d: invalid boolean value '%s' for key '%s'",
                     line_num, val_str, opt->key);
            return INI_ERR_INVALID_VALUE;
        }
    } else if (opt->type == INI_TYPE_CHAR) {
        if (val_str[0] == '\0' || val_str[1] != '\0') {
            snprintf(p->error, sizeof(p->error), "line %d: invalid char value '%s' for key '%s' (expected 1 char)",
                     line_num, val_str, opt->key);
            return INI_ERR_INVALID_VALUE;
        }
        *(char *)(opt->target) = val_str[0];
    } else if (opt->type == INI_TYPE_STR) {
        if (opt->max_size == 0) {
            snprintf(p->error, sizeof(p->error), "line %d: destination buffer for key '%s' has 0 size",
                     line_num, opt->key);
            return INI_ERR_INVALID_VALUE;
        }
        strncpy((char *)(opt->target), val_str, opt->max_size - 1);
        ((char *)(opt->target))[opt->max_size - 1] = '\0';
    } else if (opt->type == INI_TYPE_INT) {
        char *endptr = NULL;
        errno = 0;
        long v = strtol(val_str, &endptr, 10);
        if (errno != 0 || *endptr != '\0' || endptr == val_str) {
            snprintf(p->error, sizeof(p->error), "line %d: invalid integer value '%s' for key '%s'",
                     line_num, val_str, opt->key);
            return INI_ERR_INVALID_VALUE;
        }
        *(int *)(opt->target) = (int)v;
    } else if (opt->type == INI_TYPE_FLOAT) {
        char *endptr = NULL;
        errno = 0;
        double v = strtod(val_str, &endptr);
        if (errno != 0 || *endptr != '\0' || endptr == val_str) {
            snprintf(p->error, sizeof(p->error), "line %d: invalid float value '%s' for key '%s'",
                     line_num, val_str, opt->key);
            return INI_ERR_INVALID_VALUE;
        }
        *(double *)(opt->target) = v;
    }

    if (!ini_check_choice(opt, val_str)) {
        snprintf(p->error, sizeof(p->error), "line %d: invalid choice '%s' for key '%s' (allowed: [%s])",
                 line_num, val_str, opt->key, opt->choices);
        return INI_ERR_INVALID_VALUE;
    }

    return INI_OK;
}

static void ini_strip_inline_comment(char *str) {
    bool in_quotes = false;
    char quote_char = '\0';

    for (size_t i = 0; str[i] != '\0'; i++) {
        if (!in_quotes && (str[i] == '"' || str[i] == '\'')) {
            in_quotes = true;
            quote_char = str[i];
        } else if (in_quotes && str[i] == quote_char) {
            in_quotes = false;
            quote_char = '\0';
        } else if (!in_quotes && (str[i] == ';' || str[i] == '#')) {
            str[i] = '\0';
            break;
        }
    }
}

static char *ini_unquote(char *str) {
    size_t len = strlen(str);
    if (len >= 2) {
        if ((str[0] == '"' && str[len - 1] == '"') ||
            (str[0] == '\'' && str[len - 1] == '\'')) {
            str[len - 1] = '\0';
            return str + 1;
        }
    }
    return str;
}

IniStatus ini_parse_string(IniParser *p, const char *content) {
    if (p == NULL || content == NULL) {
        if (p) snprintf(p->error, sizeof(p->error), "Null parser or content pointer");
        return INI_ERR_INVALID_VALUE;
    }
    p->error[0] = '\0';

    for (size_t i = 0; i < p->count; i++) {
        p->options[i].is_set = false;
    }

    char current_section[128] = {0};
    int line_num = 0;

    const char *line_start = content;
    while (*line_start) {
        line_num++;
        const char *line_end = strchr(line_start, '\n');
        size_t line_len = line_end ? (size_t)(line_end - line_start) : strlen(line_start);

        char line_buf[512];
        if (line_len >= sizeof(line_buf)) {
            snprintf(p->error, sizeof(p->error), "line %d: line exceeds maximum length of %zu characters",
                     line_num, sizeof(line_buf) - 1);
            return INI_ERR_SYNTAX;
        }
        memcpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';

        line_start = line_end ? line_end + 1 : line_start + line_len;

        ini_strip_inline_comment(line_buf);
        char *line = ini_trim(line_buf);

        if (*line == '\0') continue;

        if (*line == '[') {
            char *closing = strrchr(line, ']');
            if (!closing || *(closing + 1) != '\0') {
                snprintf(p->error, sizeof(p->error), "line %d: malformed section header '%s'", line_num, line);
                return INI_ERR_SYNTAX;
            }
            *closing = '\0';
            char *sec_name = ini_trim(line + 1);

            strncpy(current_section, sec_name, sizeof(current_section) - 1);
            current_section[sizeof(current_section) - 1] = '\0';
            continue;
        }

        char *delimiter = strchr(line, '=');
        if (!delimiter) delimiter = strchr(line, ':');

        if (!delimiter) {
            snprintf(p->error, sizeof(p->error), "line %d: expected '=' or ':' delimiter in '%s'", line_num, line);
            return INI_ERR_SYNTAX;
        }

        *delimiter = '\0';
        char *raw_key = ini_trim(line);
        char *raw_val = ini_trim(delimiter + 1);
        char *val = ini_unquote(raw_val);

        if (*raw_key == '\0') {
            snprintf(p->error, sizeof(p->error), "line %d: empty key definition", line_num);
            return INI_ERR_SYNTAX;
        }

        IniOption *matched = ini_find_option(p, current_section, raw_key);
        if (!matched) {
            if (current_section[0] != '\0') {
                snprintf(p->error, sizeof(p->error), "line %d: unknown key '%s' in section '[%s]'",
                         line_num, raw_key, current_section);
            } else {
                snprintf(p->error, sizeof(p->error), "line %d: unknown global key '%s'",
                         line_num, raw_key);
            }
            return INI_ERR_UNKNOWN_KEY;
        }

        IniStatus st = ini_assign_val(p, matched, val, line_num);
        if (st != INI_OK) return st;
    }

    for (size_t i = 0; i < p->count; i++) {
        if (p->options[i].required && !p->options[i].is_set) {
            if (p->options[i].section && p->options[i].section[0] != '\0') {
                snprintf(p->error, sizeof(p->error), "missing mandatory option: [%s] %s",
                         p->options[i].section, p->options[i].key);
            } else {
                snprintf(p->error, sizeof(p->error), "missing mandatory global option: %s",
                         p->options[i].key);
            }
            return INI_ERR_MISSING_REQUIRED;
        }
    }

    return INI_OK;
}

IniStatus ini_parse_file(IniParser *p, const char *filename) {
    if (p == NULL || filename == NULL) {
        if (p) snprintf(p->error, sizeof(p->error), "Null parser or filename pointer");
        return INI_ERR_INVALID_VALUE;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        snprintf(p->error, sizeof(p->error), "Failed to open file '%s': %s", filename, strerror(errno));
        return INI_ERR_FILE_IO;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        snprintf(p->error, sizeof(p->error), "Failed to read size of '%s'", filename);
        return INI_ERR_FILE_IO;
    }

    char *buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        snprintf(p->error, sizeof(p->error), "Memory allocation failed reading '%s'", filename);
        return INI_ERR_ALLOC;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    IniStatus st = ini_parse_string(p, buffer);
    free(buffer);
    return st;
}

#endif // LIBINI_IMPLEMENTED
#endif // LIBINI_IMPLEMENTATION
