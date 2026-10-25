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
  size_t count;
  size_t capacity;
  size_t tombstone;
}hashmap;

extern void hashmap_init(hashmap *hm);
extern void hashmap_init_wcapacity(hashmap *hm, size_t capacity);
extern int hashmap_set(hashmap *hm, const char *key, void *value);
//mettere una macro hashmap_get_type(hashmap *hm, const char *key, type) ???
extern void *hashmap_get(hashmap *hm, const char *key);
#define hashmap_get_type(hm, key, type) ((type *)hashmap_get((hm), (key)))
static inline void *hashmap_get_or(hashmap *hm, const char *key, void *default_val) {
    void *val = hashmap_get(hm, key);
    return val ? val : default_val;
}
#define hashmap_get_or_type(hm, key, type, default_val) \
    ((type *)hashmap_get_or((hm), (key), (void *)(default_val)))
extern int hashmap_has(hashmap *hm, const char *key);
extern int hashmap_remove(hashmap *hm, const char *key);
extern void hashmap_destroy(hashmap *hm);
extern int hashmap_next(hashmap *hm, int *iter, char **key, void **value);
typedef void (*hashmap_callback)(const char *key, void *value, void *user_data);
extern void hashmap_foreach(const hashmap *hm, hashmap_callback cb, void *user_data);
#define hashmap_for_each(hm, it_var, key_var, val_var) \
    for (int it_var = 0; hashmap_next((hm), &(it_var), (char **)&(key_var), (void **)&(val_var)); )
#define hashmap_count(hm)    ((hm)->count)
#define hashmap_is_empty(hm) ((hm)->count == 0)
#endif
