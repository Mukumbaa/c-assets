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

extern int arena_init(Arena *arena, size_t capacity);
extern int arena_init_with_buffer(Arena *arena, uint8_t buffer[], size_t capacity);
extern void arena_free(Arena *arena);
extern size_t arena_used_bytes(const Arena *arena);
extern size_t arena_free_bytes(const Arena *arena);
extern size_t arena_capacity(const Arena *arena);
extern void *arena_alloc(Arena *arena, size_t size);
extern void *arena_alloc_zero(Arena *arena, size_t size);
extern void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

#define arena_alloc_type(arena, type) \
  ((type *)arena_alloc((arena), sizeof(type)))

#define arena_alloc_buffer(arena, type, size)           \
  (((size) <= SIZE_MAX / sizeof(type))                  \
  ? (type *)arena_alloc((arena), (size) * sizeof(type)) \
  : NULL)

extern void arena_reset(Arena *arena);
extern ArenaMarker arena_save(const Arena *arena);
extern int arena_restore(Arena *arena, ArenaMarker marker);
extern TempArena arena_temp_begin(Arena *arena);
extern int arena_temp_end(TempArena temp);
extern char *arena_dup_str(Arena *arena, const char *str);
extern void *arena_dup_data(Arena *arena, const void *data, size_t size);

#endif
