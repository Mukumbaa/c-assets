#ifndef STR_LIB_H
#define STR_LIB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CASE_SENSITIVE 1
#define NO_CASE_SENSITIVE 0

typedef struct {
  char *str;
  size_t len;
  size_t capacity;
} String;

extern String str_init(const char *s);
extern String str_init_with_cap(size_t initial_capacity);
extern String str_empty(void);
extern String str_from_fmt(const char *fmt, ...);
extern String str_dup(const String *src);
extern void   str_destroy(String *str);
extern void   str_clear(String *str);
extern void   str_reset(String *str);

extern bool   str_reserve(String *str, size_t min_capacity);
extern bool   str_shrink_to_fit(String *str);
extern bool   str_cat(String *dest, const String *src, size_t index);
extern bool   str_append_cstr(String *dest, const char *src);
extern bool   str_append(String *dest, const String *src);
extern bool   str_append_fmt(String *dest, const char *fmt, ...);
extern bool   str_push_char(String *dest, char c);
extern bool   str_pop(String *dest, char *out_char);
extern bool   str_cpy(String *dest, const String *src);

extern String str_substr(const String *s, size_t start, size_t count);
extern bool   str_remove(String *s, size_t index, size_t count);
extern bool   str_replace(String *s, const char *old_sub, const char *new_sub);

extern void   str_trim(String *s);
extern void   str_ltrim(String *s);
extern void   str_rtrim(String *s);
extern void   str_to_upper(String *s);
extern void   str_reverse(String *s);

extern int       str_cmp(const String *str1, const String *str2);
extern int       str_cmp_cstr(const String *str1, const char *str2);
extern bool      str_equals(const String *str1, const String *str2);
extern bool      str_equals_cstr(const String *str1, const char *str2);
extern bool      str_is_empty(const String *s);
extern bool      str_starts_with(const String *s, const char *prefix);
extern bool      str_ends_with(const String *s, const char *suffix);
extern ptrdiff_t str_contains(const String *str, const String *pattern, int is_case_sensitive);
extern ptrdiff_t str_contains_cstr(const String *str, const char *pattern, int is_case_sensitive);
extern ptrdiff_t str_find_last(const String *str, const char *pattern, int is_case_sensitive);

extern int    str_split(String **arr, const String *str, char delimiter);
extern bool   str_join(String *dest, const String *arr, int count, const char *separator);
extern void   str_free_split(String *arr, int count);

#endif
