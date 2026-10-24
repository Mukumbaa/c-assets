#ifndef LIBMAP_H
#define LIBMAP_H

#include <stddef.h>
#include <stdint.h>



typedef struct{
  // key == NULL -> empty | key = (char *) -1 -> tombstone
  char *key;
  void *value;
  uint32_t hash;
}hashmap_entry;

typedef struct{
  hashmap_entry *he;
  int count;
  int capacity;
  int tombstone;
}hashmap;

extern void hashmap_init(hashmap *hm);
extern int hashmap_set(hashmap *hm, const char *key, void *value);
extern void *hashmap_get(hashmap *hm, const char *key);
extern int hashmap_has(hashmap *hm, const char *key);
extern int hashmap_remove(hashmap *hm, const char *key);
extern void hashmap_destroy(hashmap *hm);
extern int hashmap_next(hashmap *hm, int *iter, char **key, void **value);
typedef void (*hashmap_callback)(const char *key, void *value, void *user_data);
extern void hashmap_foreach(const hashmap *hm, hashmap_callback cb, void *user_data);
#endif
