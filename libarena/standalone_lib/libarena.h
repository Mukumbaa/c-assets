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

typedef size_t ArenaMarker;

typedef struct{
  uint8_t *buffer;
  size_t capacity;
  size_t offset;
}Arena;

LIBARENA_DEF int arena_init(Arena *arena, size_t capacity);
LIBARENA_DEF void arena_free(Arena *arena);
LIBARENA_DEF void *arena_alloc(Arena *arena, size_t size);
LIBARENA_DEF void *arena_alloc_zero(Arena *arena, size_t size);
LIBARENA_DEF void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);
LIBARENA_DEF void arena_reset(Arena *arena);
LIBARENA_DEF ArenaMarker arena_save(const Arena *arena);
LIBARENA_DEF void arena_restore(Arena *arena, ArenaMarker marker);

#endif

#ifdef LIBARENA_IMPLEMENTATION
#ifndef LIBARENA_IMPLEMENTED
#define LIBARENA_IMPLEMENTED



#endif // LIBARENA_IMPLEMENTED
#endif // LIBARENA_IMPLEMENTATION
