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

