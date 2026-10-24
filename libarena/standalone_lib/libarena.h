/* 
 * Single-header library
 * 
 * In EXACTLY ONE source file, define LIBARENA_IMPLEMENTATION before including this file:
 * #define LIBARENA_IMPLEMENTATION
 * #include "libarena.h"
 */

#ifndef LIBARENA_DEF
#ifdef __cplusplus
#define LIBARENA_DEF extern "C"
#else
#define LIBARENA_DEF extern
#endif
#endif

#ifndef LIBARENA_H
#define LIBARENA_H

#include <stdint.h>
#include <stddef.h>

#define ARENA_KB(n) ((size_t)(n) * 1024ULL)
#define ARENA_MB(n) (ARENA_KB(n) * 1024ULL)
#define ALIGNMENT 16

typedef size_t ArenaMarker;

typedef struct{
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
  int can_free;
}Arena;

typedef struct{
  Arena *arena;
  ArenaMarker marker;
}TempArena;

LIBARENA_DEF int arena_init(Arena *arena, size_t capacity);
LIBARENA_DEF int arena_init_with_buffer(Arena *arena, uint8_t buffer[], size_t capacity);
LIBARENA_DEF void arena_free(Arena *arena);
LIBARENA_DEF size_t arena_used_bytes(const Arena *arena);
LIBARENA_DEF size_t arena_free_bytes(const Arena *arena);
LIBARENA_DEF size_t arena_capacity(const Arena *arena);
LIBARENA_DEF void *arena_alloc(Arena *arena, size_t size);
LIBARENA_DEF void *arena_alloc_zero(Arena *arena, size_t size);
LIBARENA_DEF void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

#define arena_alloc_type(arena, type) \
  ((type *)arena_alloc((arena), sizeof(type)))

#define arena_alloc_buffer(arena, type, size)           \
  (((size) <= SIZE_MAX / sizeof(type))                  \
  ? (type *)arena_alloc((arena), (size) * sizeof(type)) \
  : NULL)

LIBARENA_DEF void arena_reset(Arena *arena);
LIBARENA_DEF ArenaMarker arena_save(const Arena *arena);
LIBARENA_DEF int arena_restore(Arena *arena, ArenaMarker marker);
LIBARENA_DEF TempArena arena_temp_begin(Arena *arena);
LIBARENA_DEF int arena_temp_end(TempArena temp);
LIBARENA_DEF char *arena_dup_str(Arena *arena, const char *str);
LIBARENA_DEF void *arena_dup_data(Arena *arena, const void *data, size_t size);

#endif

#ifdef LIBARENA_IMPLEMENTATION
#ifndef LIBARENA_IMPLEMENTED
#define LIBARENA_IMPLEMENTED

#include <stdlib.h>
#include <string.h>

LIBARENA_DEF int arena_init(Arena *arena, size_t capacity){
  if(arena == NULL){
    return 1;
  }
  if(capacity == 0){
    return 1;
  }
  arena->buffer = malloc(capacity);
  if(arena->buffer == NULL){
    arena->capacity = 0;
    arena->offset = 0;
    arena->can_free = 0;
    return 1;
  }
  arena->capacity = capacity;
  arena->offset = 0;
  arena->can_free = 1;
  return 0;
}

LIBARENA_DEF int arena_init_with_buffer(Arena *arena, uint8_t buffer[], size_t capacity){
  if(arena == NULL){
    return 1;
  }
  if(capacity == 0){
    return 1;
  }
  if(buffer == NULL){
    return 1;
  }
  arena->buffer = buffer;
  arena->offset = 0;
  arena->capacity = capacity;
  arena->can_free = 0;
  return 0;
}
LIBARENA_DEF void arena_free(Arena *arena){
  if(arena == NULL){
    return;
  }
  if(arena->can_free){
    free(arena->buffer);
  }
  arena->buffer = NULL;
  arena->capacity = 0;
  arena->offset = 0;
}

LIBARENA_DEF size_t arena_used_bytes(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->offset;
}
LIBARENA_DEF size_t arena_free_bytes(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->capacity - arena->offset;
}
LIBARENA_DEF size_t arena_capacity(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->capacity;
}
LIBARENA_DEF void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment){
  if(arena == NULL || arena->buffer == NULL){
    return NULL;
  }
  if(size == 0){
    return NULL;
  }
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  uintptr_t p = (uintptr_t)arena->buffer + arena->offset;
  uintptr_t mod = p % alignment;
  size_t padding = 0;

  if (mod != 0) {
    padding = alignment - mod;
  }
  if (padding > arena->capacity - arena->offset) {
    return NULL;
  }
  size_t start = arena->offset + padding;

  if (size > arena->capacity - start) {
    return NULL;
  }



  arena->offset = start + size;
  return &arena->buffer[start];
}

LIBARENA_DEF void *arena_alloc(Arena *arena, size_t size){
  return arena_alloc_aligned(arena, size, ALIGNMENT);
}

LIBARENA_DEF void *arena_alloc_zero(Arena *arena, size_t size){
  void *ptr = arena_alloc_aligned(arena, size, ALIGNMENT);
  if(ptr == NULL){
    return NULL;
  }

  memset(ptr, 0, size);
  return ptr;
}

LIBARENA_DEF void arena_reset(Arena *arena){
  if(arena == NULL){
    return;
  }
  arena->offset = 0;
}
LIBARENA_DEF ArenaMarker arena_save(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->offset;
}
LIBARENA_DEF int arena_restore(Arena *arena, ArenaMarker marker){
  if(arena == NULL){
    return 1;
  }
  if(marker > arena->offset || marker > arena->capacity){
    return 1;
  }
  arena->offset = marker;
  return 0;
}

LIBARENA_DEF TempArena arena_temp_begin(Arena *arena){
  TempArena ta;
  if(arena == NULL){
    ta.arena = NULL;
    ta.marker = 0;
    return ta;
  }
  ta.arena = arena;
  ta.marker = arena->offset;
  return ta;
}
LIBARENA_DEF int arena_temp_end(TempArena temp){
  return arena_restore(temp.arena, temp.marker);
}

LIBARENA_DEF void *arena_dup_data(Arena *arena, const void *data, size_t size){
  if(arena == NULL || data == NULL){
    return NULL;
  }


  void *ptr = arena_alloc_aligned(arena, size, ALIGNMENT);
  if(ptr == NULL){
    return NULL;
  }
  memcpy(ptr, data, size);

  return ptr;

}

LIBARENA_DEF char *arena_dup_str(Arena *arena, const char *str){
  if(arena == NULL || str == NULL){
    return NULL;
  }

  size_t i = strlen(str);

  void *ptr = arena_alloc_aligned(arena, i + 1, ALIGNMENT);
  if(ptr == NULL){
    return NULL;
  }
  memcpy((char *)ptr, str, i + 1);


  return ptr;
}

#endif // LIBARENA_IMPLEMENTED
#endif // LIBARENA_IMPLEMENTATION
