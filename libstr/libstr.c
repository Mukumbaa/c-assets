#include "libstr.h"
#include <ctype.h>

// calculate buffer growth capacity using a 1.5x factor
static size_t get_next_capacity(size_t current, size_t needed) {
    // guard against overflow when reserving room for null terminator
    if (needed > SIZE_MAX - 1) return 0;

    size_t cap = (current == 0) ? 16 : current;
    while (cap < needed) {
        // check for overflow before expanding capacity by 50%
        if (cap > SIZE_MAX - (cap >> 1)) {
            cap = needed;
            break;
        }
        cap += (cap >> 1);
    }
    return cap;
}

// allocate an empty string with default initial capacity
extern String str_empty(void) {
    String s;
    s.capacity = 16;
    s.len = 0;
    s.str = malloc(s.capacity);
    if (s.str) {
        s.str[0] = '\0';
    } else {
        s.capacity = 0;
    }
    return s;
}

// allocate an empty string with requested minimum capacity
extern String str_init_with_cap(size_t initial_capacity) {
    String str;
    str.len = 0;
    str.capacity = (initial_capacity < 16) ? 16 : initial_capacity;
    str.str = malloc(str.capacity);
    if (str.str) {
        str.str[0] = '\0';
    } else {
        str.capacity = 0;
    }
    return str;
}

// create a dynamic string by copying a c-string
extern String str_init(const char *s) {
    if (!s) return str_empty();

    size_t actual_len = strlen(s);
    size_t initial_cap = (actual_len + 1 < 16) ? 16 : actual_len + 1;

    String str;
    str.capacity = initial_cap;
    str.str = malloc(str.capacity);
    if (!str.str) {
        str.len = 0;
        str.capacity = 0;
        return str;
    }

    memcpy(str.str, s, actual_len);
    str.str[actual_len] = '\0';
    str.len = actual_len;
    return str;
}

// duplicate an existing dynamic string into a new heap allocation
extern String str_dup(const String *src) {
    if (!src || !src->str) return str_empty();
    String copy = str_init_with_cap(src->len + 1);
    if (!copy.str) return copy;
    memcpy(copy.str, src->str, src->len);
    copy.str[src->len] = '\0';
    copy.len = src->len;
    return copy;
}

// create a new string from a printf-style format string
extern String str_from_fmt(const char *fmt, ...) {
    if (!fmt) return str_empty();

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    // dry run with vsnprintf to determine required buffer size
    int written = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (written < 0) {
        va_end(args);
        return str_empty();
    }

    String s = str_init_with_cap((size_t)written + 1);
    if (s.str) {
        vsnprintf(s.str, s.capacity, fmt, args);
        s.len = (size_t)written;
    }
    va_end(args);
    return s;
}

// free heap buffer and zero out string fields
extern void str_destroy(String *str) {
    if (!str) return;
    if (str->str) {
        free(str->str);
        str->str = NULL;
    }
    str->len = 0;
    str->capacity = 0;
}

// clear string contents without releasing the allocated buffer
extern void str_clear(String *str) {
    if (!str || !str->str) return;
    str->str[0] = '\0';
    str->len = 0;
}

// reset string length to zero while keeping or allocating the buffer
extern void str_reset(String *str) {
    if (!str) return;
    if (str->str && str->capacity > 0) {
        str->str[0] = '\0';
        str->len = 0;
    } else {
        *str = str_empty();
    }
}

bool str_reserve(String *str, size_t space) {
    if (!str) return false;
    if (space == 0) return true;

    if (str->capacity > str->len && (str->capacity - str->len - 1) >= space) {
        return true;
    }

    if (SIZE_MAX - str->len - 1 < space) return false;
    size_t needed_total = str->len + space + 1;

    size_t next_cap = get_next_capacity(str->capacity, needed_total);
    if (next_cap == 0) return false;

    char *new_str = realloc(str->str, next_cap);
    if (!new_str) return false;

    str->str = new_str;
    str->capacity = next_cap;
    return true;
}

// shrink allocated capacity to fit current length plus null terminator
extern bool str_shrink_to_fit(String *str) {
    if (!str || !str->str) return false;
    size_t needed = str->len + 1;
    if (str->capacity <= needed) return true;

    char *new_str = realloc(str->str, needed);
    if (!new_str) return false;

    str->str = new_str;
    str->capacity = needed;
    return true;
}

// append a single character to the end, reallocating if needed
extern bool str_push_char(String *dest, char c) {
    if (!dest) return false;

    if (dest->len + 2 > dest->capacity) {
        size_t next_cap = get_next_capacity(dest->capacity, dest->len + 2);
        if (!str_reserve(dest, next_cap)) return false;
    }

    dest->str[dest->len] = c;
    dest->len++;
    dest->str[dest->len] = '\0';
    return true;
}

// pop the last character from the string and optionally store it
extern bool str_pop(String *dest, char *out_char) {
    if (!dest || dest->len == 0) return false;
    dest->len--;
    if (out_char) {
        *out_char = dest->str[dest->len];
    }
    dest->str[dest->len] = '\0';
    return true;
}

// insert another dynamic string at index with aliasing protection
extern bool str_cat(String *dest, const String *src, size_t index) {
    if (!dest || !src || !src->str || index > dest->len) return false;
    if (src->len == 0) return true;

    // guard against integer overflow on combined length
    if (SIZE_MAX - dest->len < src->len) return false;
    size_t total_len = dest->len + src->len;

    // guard against overflow when adding space for null terminator
    if (total_len == SIZE_MAX) return false;

    // check if src->str points into dest's buffer to prevent use-after-free on realloc
    bool is_aliased = (src->str >= dest->str && src->str < dest->str + dest->capacity);
    char *temp_src = NULL;

    if (is_aliased) {
        temp_src = malloc(src->len);
        if (!temp_src) return false;
        memcpy(temp_src, src->str, src->len);
    }

    const char *src_bytes = temp_src ? temp_src : src->str;

    if (total_len + 1 > dest->capacity) {
        size_t next_cap = get_next_capacity(dest->capacity, total_len + 1);
        if (next_cap == 0 || !str_reserve(dest, next_cap)) {
            if (temp_src) free(temp_src);
            return false;
        }
    }

    if (index < dest->len) {
        memmove(dest->str + index + src->len, dest->str + index, dest->len - index);
    }

    memcpy(dest->str + index, src_bytes, src->len);

    if (temp_src) free(temp_src);

    dest->len = total_len;
    dest->str[dest->len] = '\0';
    return true;
}

// append a dynamic string to the end of dest
extern bool str_append(String *dest, const String *src) {
    if (!dest) return false;
    return str_cat(dest, src, dest->len);
}

// append a raw c-string to the end of dest
extern bool str_append_cstr(String *dest, const char *src) {
    if (!dest || !src) return false;
    size_t slen = strlen(src);
    String tmp = { .str = (char*)src, .len = slen, .capacity = slen + 1 };
    return str_cat(dest, &tmp, dest->len);
}

// append formatted text using printf-style arguments
extern bool str_append_fmt(String *dest, const char *fmt, ...) {
    if (!dest || !fmt) return false;

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    int written = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (written < 0) {
        va_end(args);
        return false;
    }

    size_t needed_len = dest->len + (size_t)written;
    if (needed_len + 1 > dest->capacity) {
        size_t next_cap = get_next_capacity(dest->capacity, needed_len + 1);
        if (!str_reserve(dest, next_cap)) {
            va_end(args);
            return false;
        }
    }

    vsnprintf(dest->str + dest->len, dest->capacity - dest->len, fmt, args);
    va_end(args);

    dest->len = needed_len;
    dest->str[dest->len] = '\0';
    return true;
}

// copy source string into destination, overwriting existing contents
extern bool str_cpy(String *dest, const String *src) {
    if (!dest || !src) return false;
    if (dest == src) return true;

    if (src->len + 1 > dest->capacity) {
        if (!str_reserve(dest, src->len + 1)) return false;
    }

    memcpy(dest->str, src->str, src->len);
    dest->len = src->len;
    dest->str[dest->len] = '\0';
    return true;
}

// extract a substring of count characters starting at start index
extern String str_substr(const String *s, size_t start, size_t count) {
    if (!s || !s->str || start >= s->len || count == 0) {
        return str_empty();
    }

    if (start + count > s->len) {
        count = s->len - start;
    }

    String sub = str_init_with_cap(count + 1);
    if (!sub.str) return str_empty();

    memcpy(sub.str, s->str + start, count);
    sub.str[count] = '\0';
    sub.len = count;
    return sub;
}

// delete count characters in place starting from index
extern bool str_remove(String *s, size_t index, size_t count) {
    if (!s || !s->str || index >= s->len || count == 0) return false;

    if (index + count >= s->len) {
        s->len = index;
        s->str[s->len] = '\0';
        return true;
    }

    size_t chars_after = s->len - (index + count);
    memmove(s->str + index, s->str + index + count, chars_after);
    s->len -= count;
    s->str[s->len] = '\0';
    return true;
}

// replace all occurrences of old_sub with new_sub using a temp buffer
extern bool str_replace(String *s, const char *old_sub, const char *new_sub) {
    if (!s || !s->str || !old_sub || !new_sub) return false;
    size_t old_len = strlen(old_sub);
    if (old_len == 0) return false;
    size_t new_len = strlen(new_sub);

    String result = str_init_with_cap(s->capacity);
    if (!result.str) return false;

    String repl_token = { .str = (char*)new_sub, .len = new_len, .capacity = new_len + 1 };

    size_t i = 0;
    while (i < s->len) {
        if (i + old_len <= s->len && memcmp(s->str + i, old_sub, old_len) == 0) {
            if (!str_cat(&result, &repl_token, result.len)) {
                str_destroy(&result);
                return false;
            }
            i += old_len;
        } else {
            if (!str_push_char(&result, s->str[i])) {
                str_destroy(&result);
                return false;
            }
            i++;
        }
    }

    str_destroy(s);
    *s = result;
    return true;
}

// strip leading whitespace characters in place
extern void str_ltrim(String *s) {
    if (!s || !s->str || s->len == 0) return;

    size_t start = 0;
    while (start < s->len && isspace((unsigned char)s->str[start])) {
        start++;
    }

    if (start == 0) return;

    size_t new_len = s->len - start;
    memmove(s->str, s->str + start, new_len);
    s->len = new_len;
    s->str[s->len] = '\0';
}

// strip trailing whitespace characters in place
extern void str_rtrim(String *s) {
    if (!s || !s->str || s->len == 0) return;

    size_t end = s->len;
    while (end > 0 && isspace((unsigned char)s->str[end - 1])) {
        end--;
    }

    s->len = end;
    s->str[s->len] = '\0';
}

// strip whitespace from both ends
extern void str_trim(String *s) {
    str_rtrim(s);
    str_ltrim(s);
}

// convert all characters to lowercase in place
extern void str_to_lower(String *s) {
    if (!s || !s->str) return;
    for (size_t i = 0; i < s->len; i++) {
        s->str[i] = (char)tolower((unsigned char)s->str[i]);
    }
}

// convert all characters to uppercase in place
extern void str_to_upper(String *s) {
    if (!s || !s->str) return;
    for (size_t i = 0; i < s->len; i++) {
        s->str[i] = (char)toupper((unsigned char)s->str[i]);
    }
}

// reverse the string in place using two pointers
extern void str_reverse(String *s) {
    if (!s || !s->str || s->len < 2) return;
    size_t left = 0;
    size_t right = s->len - 1;
    while (left < right) {
        char tmp = s->str[left];
        s->str[left] = s->str[right];
        s->str[right] = tmp;
        left++;
        right--;
    }
}

// lexicographically compare two dynamic strings
extern int str_cmp(const String *str1, const String *str2) {
    if (!str1 || !str2) return (str1 == str2) ? 0 : (str1 ? 1 : -1);

    size_t min_l = (str1->len < str2->len) ? str1->len : str2->len;
    int res = memcmp(str1->str, str2->str, min_l);
    if (res != 0) return res;

    if (str1->len < str2->len) return -1;
    if (str1->len > str2->len) return 1;
    return 0;
}

// lexicographically compare a dynamic string with a raw c-string
extern int str_cmp_cstr(const String *str1, const char *str2) {
    if (!str1 || !str2) return (str1 && !str2) ? 1 : (!str1 && str2 ? -1 : 0);

    size_t i = 0;
    while (i < str1->len && str2[i] != '\0') {
        unsigned char c1 = (unsigned char)str1->str[i];
        unsigned char c2 = (unsigned char)str2[i];
        if (c1 != c2) {
            return (int)c1 - (int)c2;
        }
        i++;
    }

    if (i < str1->len) return 1;
    if (str2[i] != '\0') return -1;
    return 0;
}

// check if two dynamic strings have identical length and content
extern bool str_equals(const String *str1, const String *str2) {
    if (!str1 || !str2) return false;
    if (str1->len != str2->len) return false;
    return memcmp(str1->str, str2->str, str1->len) == 0;
}

// check if dynamic string matches raw c-string
extern bool str_equals_cstr(const String *str1, const char *str2) {
    if (!str1 || !str1->str || !str2) return false;
    size_t len2 = strlen(str2);
    if (str1->len != len2) return false;
    return memcmp(str1->str, str2, len2) == 0;
}

// return true if string pointer is null or string length is zero
extern bool str_is_empty(const String *s) {
    return (!s || s->len == 0);
}

// check if string starts with the given prefix
extern bool str_starts_with(const String *s, const char *prefix) {
    if (!s || !s->str || !prefix) return false;
    size_t prefix_len = strlen(prefix);
    if (prefix_len > s->len) return false;
    return memcmp(s->str, prefix, prefix_len) == 0;
}

// check if string ends with the given suffix
extern bool str_ends_with(const String *s, const char *suffix) {
    if (!s || !s->str || !suffix) return false;
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s->len) return false;
    return memcmp(s->str + (s->len - suffix_len), suffix, suffix_len) == 0;
}

// find first occurrence index of pattern, returns -1 if not found
extern ptrdiff_t str_contains(const String *str, const String *pattern, int is_case_sensitive) {
    if (!str || !pattern || pattern->len == 0 || str->len < pattern->len) return -1;

    for (size_t i = 0; i <= str->len - pattern->len; i++) {
        size_t j = 0;
        while (j < pattern->len) {
            unsigned char c1 = (unsigned char)str->str[i + j];
            unsigned char c2 = (unsigned char)pattern->str[j];
            if (is_case_sensitive ? (c1 != c2) : (tolower(c1) != tolower(c2))) {
                break;
            }
            j++;
        }
        if (j == pattern->len) {
            // guard against returning an index that overflows ptrdiff_t
            if (i > (size_t)PTRDIFF_MAX) return -1;
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

// find first occurrence of a raw c-string pattern
extern ptrdiff_t str_contains_cstr(const String *str, const char *pattern, int is_case_sensitive) {
    if (!str || !pattern) return -1;
    size_t plen = strlen(pattern);
    String p = { .str = (char*)pattern, .len = plen, .capacity = plen + 1 };
    return str_contains(str, &p, is_case_sensitive);
}

// scan backwards to find last occurrence of pattern
extern ptrdiff_t str_find_last(const String *str, const char *pattern, int is_case_sensitive) {
    if (!str || !pattern || !str->str) return -1;
    size_t plen = strlen(pattern);
    if (plen == 0 || str->len < plen) return -1;

    for (size_t i = str->len - plen + 1; i > 0; i--) {
        size_t idx = i - 1;
        size_t j = 0;
        while (j < plen) {
            unsigned char c1 = (unsigned char)str->str[idx + j];
            unsigned char c2 = (unsigned char)pattern[j];
            if (is_case_sensitive ? (c1 != c2) : (tolower(c1) != tolower(c2))) {
                break;
            }
            j++;
        }
        if (j == plen) {
            if (idx > (size_t)PTRDIFF_MAX) return -1;
            return (ptrdiff_t)idx;
        }
    }
    return -1;
}

// tokenize string by delimiter into an array of dynamic strings
extern int str_split(String **arr, const String *str, char delimiter) {
    if (!arr || !str || !str->str) return -1;

    size_t capacity = 8;
    int count = 0;
    String *tokens = malloc(capacity * sizeof(String));
    if (!tokens) return -1;

    size_t start = 0;
    for (size_t i = 0; i <= str->len; i++) {
        if (i == str->len || str->str[i] == delimiter) {
            size_t token_len = i - start;

            if ((size_t)count >= capacity) {
                // guard against overflow before doubling array capacity
                if (capacity > SIZE_MAX / 2 || (capacity * 2) > SIZE_MAX / sizeof(String)) {
                    str_free_split(tokens, count);
                    return -1;
                }
                size_t new_cap = capacity * 2;

                String *temp = realloc(tokens, new_cap * sizeof(String));
                if (!temp) {
                    str_free_split(tokens, count);
                    return -1;
                }
                tokens = temp;
                capacity = new_cap;
            }

            tokens[count] = str_init_with_cap(token_len + 1);
            if (!tokens[count].str) {
                str_free_split(tokens, count);
                return -1;
            }
            memcpy(tokens[count].str, str->str + start, token_len);
            tokens[count].str[token_len] = '\0';
            tokens[count].len = token_len;
            count++;

            start = i + 1;
        }
    }

    *arr = tokens;
    return count;
}

// join an array of strings into dest with single preallocation pass
extern bool str_join(String *dest, const String *arr, int count, const char *separator) {
    if (!dest || !arr || count < 0) return false;
    str_clear(dest);
    if (count == 0) return true;

    size_t sep_len = separator ? strlen(separator) : 0;
    size_t total_len = 0;

    // pre-calculate required buffer size to allocate memory only once
    for (int i = 0; i < count; i++) {
        if (!arr[i].str) return false;
        if (SIZE_MAX - total_len < arr[i].len) return false;
        total_len += arr[i].len;
        if (i < count - 1 && sep_len > 0) {
            if (SIZE_MAX - total_len < sep_len) return false;
            total_len += sep_len;
        }
    }

    if (!str_reserve(dest, total_len + 1)) return false;

    // copy tokens and separators directly without reallocations
    for (int i = 0; i < count; i++) {
        memcpy(dest->str + dest->len, arr[i].str, arr[i].len);
        dest->len += arr[i].len;
        if (separator && sep_len > 0 && i < count - 1) {
            memcpy(dest->str + dest->len, separator, sep_len);
            dest->len += sep_len;
        }
    }
    dest->str[dest->len] = '\0';
    return true;
}

// destroy each string in the array and free the array pointer
extern void str_free_split(String *arr, int count) {
    if (!arr) return;
    for (int i = 0; i < count; i++) {
        str_destroy(&arr[i]);
    }
    free(arr);
}
