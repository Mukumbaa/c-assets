#ifndef LIBVEC_H
#define LIBVEC_H


#include <stddef.h>
#include <string.h>

typedef struct{
  size_t len;
  size_t capacity;
  size_t item_size;
  void *values;
}vector;

#define VEC_BASIC_CAP 32

extern int vec_init(vector *vec, size_t item_size);
extern int vec_init_wcapacity(vector *vec, size_t item_size, size_t capacity);
extern void vec_free(vector *vec);
extern void vec_clear(vector *vec);
extern int vec_shrink_to_fit(vector *vec);
extern int vec_reserve(vector *vec, size_t space);
extern int vec_append(vector *vec, const void *value);
extern int vec_insert_at(vector *vec, const void *value, size_t index);
extern int vec_pop(vector *vec, void *out);
extern int vec_remove_at(vector *vec, size_t index, void *out);
extern int vec_swap_remove(vector *vec, size_t index, void *out);
extern void *vec_get(const vector *vec, size_t index);
#define vec_get_type(vec, index, type) ((type *)vec_get((vec), (index)))
extern void *vec_first(const vector *vec);
extern void *vec_last(const vector *vec);

extern size_t vec_len(const vector *vec);
extern size_t vec_capacity(const vector *vec);
extern int vec_is_empty(const vector *vec);
extern void *vec_data(vector *vec);
#define vec_data_type(vec, type) ((type *)vec_data((vec)))

extern int vec_contains(const vector *vec, const void *value);

#endif
