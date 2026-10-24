#include "libarena.h"
#include <stdlib.h>
#include <string.h>

extern int arena_init(Arena *arena, size_t capacity){
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

extern int arena_init_with_buffer(Arena *arena, uint8_t buffer[], size_t capacity){
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
extern void arena_free(Arena *arena){
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

extern size_t arena_used_bytes(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->offset;
}
extern size_t arena_free_bytes(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->capacity - arena->offset;
}
extern size_t arena_capacity(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->capacity;
}
extern void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment){
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

extern void *arena_alloc(Arena *arena, size_t size){
  return arena_alloc_aligned(arena, size, ALIGNMENT);
}

extern void *arena_alloc_zero(Arena *arena, size_t size){
  void *ptr = arena_alloc_aligned(arena, size, ALIGNMENT);
  if(ptr == NULL){
    return NULL;
  }

  memset(ptr, 0, size);
  return ptr;
}

extern void arena_reset(Arena *arena){
  if(arena == NULL){
    return;
  }
  arena->offset = 0;
}
extern ArenaMarker arena_save(const Arena *arena){
  if(arena == NULL){
    return 0;
  }
  return arena->offset;
}
extern int arena_restore(Arena *arena, ArenaMarker marker){
  if(arena == NULL){
    return 1;
  }
  if(marker > arena->offset || marker > arena->capacity){
    return 1;
  }
  arena->offset = marker;
  return 0;
}

extern TempArena arena_temp_begin(Arena *arena){
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
extern int arena_temp_end(TempArena temp){
  return arena_restore(temp.arena, temp.marker);
}

extern void *arena_dup_data(Arena *arena, const void *data, size_t size){
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

extern char *arena_dup_str(Arena *arena, const char *str){
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

