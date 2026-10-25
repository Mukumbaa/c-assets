/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBVEC_IMPLEMENTATION before including this file:
 * #define LIBVEC_IMPLEMENTATION
 * #include "libvec.h"
 */

#ifndef LIBVEC_DEF
#ifdef __cplusplus
#define LIBVEC_DEF extern "C"
#else
#define LIBVEC_DEF extern
#endif
#endif

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

LIBVEC_DEF int vec_init(vector *vec, size_t item_size);
LIBVEC_DEF int vec_init_wcapacity(vector *vec, size_t item_size, size_t capacity);
LIBVEC_DEF void vec_free(vector *vec);
LIBVEC_DEF void vec_clear(vector *vec);
LIBVEC_DEF int vec_shrink_to_fit(vector *vec);
LIBVEC_DEF int vec_reserve(vector *vec, size_t space);
LIBVEC_DEF int vec_append(vector *vec, const void *value);
LIBVEC_DEF int vec_insert_at(vector *vec, const void *value, size_t index);
LIBVEC_DEF int vec_pop(vector *vec, void *out);
LIBVEC_DEF int vec_remove_at(vector *vec, size_t index, void *out);
LIBVEC_DEF int vec_swap_remove(vector *vec, size_t index, void *out);
LIBVEC_DEF void *vec_get(const vector *vec, size_t index);
#define vec_get_type(vec, index, type) ((type *)vec_get((vec), (index)))
LIBVEC_DEF void *vec_first(const vector *vec);
LIBVEC_DEF void *vec_last(const vector *vec);

LIBVEC_DEF size_t vec_len(const vector *vec);
LIBVEC_DEF size_t vec_capacity(const vector *vec);
LIBVEC_DEF int vec_is_empty(const vector *vec);
LIBVEC_DEF void *vec_data(vector *vec);
#define vec_data_type(vec, type) ((type *)vec_data((vec)))

LIBVEC_DEF int vec_contains(const vector *vec, const void *value);

#endif

#ifdef LIBVEC_IMPLEMENTATION
#ifndef LIBVEC_IMPLEMENTED
#define LIBVEC_IMPLEMENTED

#include <stdint.h>
#include <stdlib.h>


LIBVEC_DEF int vec_init(vector *vec, size_t item_size){
  if(item_size == 0){
    return 1;
  }
  if(vec == NULL){
    return 1;
  }
  vec->item_size = item_size;
  vec->capacity = VEC_BASIC_CAP;
  vec->len = 0;
  vec->values = malloc(VEC_BASIC_CAP * item_size);
  if(vec->values == NULL){
    vec->capacity = 0;
    return 1;
  }
  return 0;
}

LIBVEC_DEF int vec_init_wcapacity(vector *vec, size_t item_size, size_t capacity){
  if(item_size == 0 || capacity == 0){
    return 1;
  }
  if(vec == NULL){
    return 1;
  }
  vec->item_size = item_size;
  vec->capacity = capacity;
  vec->len = 0;
  vec->values = malloc(capacity * item_size);
  if(vec->values == NULL){
    vec->capacity = 0;
    return 1;
  }
  return 0;
}

LIBVEC_DEF void vec_free(vector *vec){
  if(vec == NULL){
    return;
  }
  free(vec->values);
  vec->values = NULL;
  vec->len = 0;
  vec->capacity = 0;
}

LIBVEC_DEF void vec_clear(vector *vec){
  if(vec == NULL){
    return;
  }
  vec->len = 0;
}

LIBVEC_DEF int vec_shrink_to_fit(vector *vec){
  if(vec == NULL){
    return 1;
  }
  if(vec->capacity == vec->len){
    return 0;
  }
  if(vec->len == 0){
    free(vec->values);
    vec->values = NULL;
    vec->capacity = 0;
    return 0;
  }


  void *temp = realloc(vec->values, vec->len * vec->item_size);
  if(temp == NULL){
    return 1;
  }

  vec->values = temp;
  vec->capacity = vec->len;
  return 0;
}

LIBVEC_DEF int vec_reserve(vector *vec, size_t space){
  if (vec == NULL) return 1;
  if (space == 0) return 0;

  if (vec->capacity - vec->len >= space) return 0;

  if (SIZE_MAX - vec->len < space) return 1;
  size_t needed = vec->len + space;

  size_t new_capacity = (vec->capacity == 0) ? VEC_BASIC_CAP : vec->capacity;
  while (new_capacity < needed) {
    if (new_capacity > SIZE_MAX / 2) {
      new_capacity = needed;
      break;
    }
    new_capacity *= 2;
  }

  if (new_capacity > SIZE_MAX / vec->item_size) return 1;

  void *temp = realloc(vec->values, new_capacity * vec->item_size);
  if (temp == NULL) {
    return 1;
  }

  vec->values = temp;
  vec->capacity = new_capacity;
  return 0;
}

LIBVEC_DEF int vec_append(vector *vec, const void *value){
  if(vec == NULL){
    return 1;
  }
  if(value == NULL){
    return 1;
  }
  
  if(vec->len == vec->capacity){
    int res = vec_reserve(vec, 1);
    if(res == 1){
      return 1;
    }
  }
  
  void *dest = (char *)vec->values + (vec->len * vec->item_size);
  memcpy(dest, value, vec->item_size);
  vec->len++;

  return 0;
}

LIBVEC_DEF int vec_insert_at(vector *vec, const void *value, size_t index){
  if(vec == NULL){
    return 1;
  }
  if(value == NULL){
    return 1;
  }
  if(index > vec->len){
    return 1;
  }

  if(index == vec->len){
    return vec_append(vec, value);
  } 
    
  if(vec->len == vec->capacity){
    int res = vec_reserve(vec, 1);
    if(res == 1){
      return 1;
    }
  }

  size_t bytes_to_move = (vec->len - index) * vec->item_size;
  void *src = (char *)vec->values + (index * vec->item_size);
  void *dest = (char *)vec->values + ((index + 1) * vec->item_size);
  memmove(dest, src, bytes_to_move);
  memcpy(src, value, vec->item_size);
  vec->len++;
  return 0;
}

LIBVEC_DEF int vec_pop(vector *vec, void *out){
  if(vec == NULL){
    return 1;
  }
  if(vec->len == 0){
    return 1;
  }
   
  vec->len--;
  if(out != NULL){
    void *src = (char *)vec->values + (vec->len * vec->item_size);
    memcpy(out, src, vec->item_size);
  }
  return 0;
}

LIBVEC_DEF int vec_remove_at(vector *vec, size_t index, void *out){
  if(vec == NULL){
    return 1;
  }
  if(index >= vec->len){
    return 1;
  }

  void *target = (char *)vec->values + (index * vec->item_size);

  if(out != NULL){
    memcpy(out, target, vec->item_size);
  }

  size_t elements_after = vec->len - 1 - index;
  if(elements_after > 0){
    void *next = (char *)vec->values + ((index + 1) * vec->item_size);
    memmove(target, next, elements_after * vec->item_size);
  }
  vec->len--;
  return 0; 
}

LIBVEC_DEF int vec_swap_remove(vector *vec, size_t index, void *out){

  if(vec == NULL){
    return 1;
  }
  if(index >= vec->len){
    return 1;
  }

  void *src = (char *)vec->values + (index * vec->item_size);
  if(out != NULL){
    memcpy(out, src, vec->item_size);
  }

  if(index < vec->len - 1){
    void *last = (char *)vec->values + ((vec->len - 1) * vec->item_size);
    memcpy(src, last, vec->item_size);
  }

  vec->len--;
  return 0;
}

LIBVEC_DEF void *vec_get(const vector *vec, size_t index){

  if(vec == NULL){
    return NULL;
  }
  if(index >= vec->len){
    return NULL;
  }

  return (char *)vec->values + (index * vec->item_size);
}
LIBVEC_DEF void *vec_first(const vector *vec){
  return vec_get(vec, 0); 
}
LIBVEC_DEF void *vec_last(const vector *vec){

  if(vec == NULL){
    return NULL;
  }
  if(vec->len == 0){
    return NULL;
  }
  return vec_get(vec, vec->len - 1); 
}

LIBVEC_DEF size_t vec_len(const vector *vec){
  if(vec == NULL){
    return 0;
  }
  return vec->len;
}
LIBVEC_DEF size_t vec_capacity(const vector *vec){
  if(vec == NULL){
    return 0;
  }
  return vec->capacity;
}
LIBVEC_DEF int vec_is_empty(const vector *vec){
  if(vec == NULL || vec->len == 0){
    return 1;
  }
  return 0;
}
LIBVEC_DEF void *vec_data(vector *vec){
  if(vec == NULL){
    return NULL;
  }
  return vec->values;
}

LIBVEC_DEF int vec_contains(const vector *vec, const void *value){
  if(vec == NULL){
    return 0;
  }
  if(value == NULL){
    return 0;
  }

  for(size_t i = 0; i < vec->len; i++){
    void *cell = (char *)vec->values + (i * vec->item_size);
    if(memcmp(cell, value, vec->item_size) == 0){
      return 1;
    }
  }
  return 0;
}

#endif // LIBVEC_IMPLEMENTED
#endif // LIBVEC_IMPLEMENTATION
