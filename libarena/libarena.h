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

extern int arena_init(Arena *arena, size_t capacity);
extern void arena_free(Arena *arena);
extern void *arena_alloc(Arena *arena, size_t size);
extern void *arena_alloc_zero(Arena *arena, size_t size);
extern void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);
extern void arena_reset(Arena *arena);
extern ArenaMarker arena_save(const Arena *arena);
extern void arena_restore(Arena *arena, ArenaMarker marker);

#endif
