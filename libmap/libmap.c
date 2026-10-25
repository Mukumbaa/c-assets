#include "libmap.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint32_t FNV_OFFSET_32 = 2166136261U;
static const uint32_t FNV_PRIME_32  = 16777619U;
static const uint64_t FNV_OFFSET_64 = 14695981039346656037ULL;
static const uint64_t FNV_PRIME_64  = 1099511628211ULL;

static uint32_t fnv1a_32(const char *key){
  uint32_t hash = FNV_OFFSET_32;
  while (*key) {
    hash ^= (unsigned char)*key++;
    hash *= FNV_PRIME_32;
  }
  return hash;
}

static uint64_t fnv1a_64(const char *key){
  uint64_t hash = FNV_OFFSET_64;
  while (*key) {
    hash ^= (unsigned char)*key++;
    hash *= FNV_PRIME_64;
  }
  return hash;
}

static size_t get_index(uint32_t hash, int capacity){
  return hash & (capacity - 1);
}

extern void hashmap_init(hashmap *hm){
  hm->capacity = 32;
  hm->count = 0;
  hm->tombstone = 0;
  hm->he = calloc(hm->capacity, sizeof(hashmap_entry));
}

extern void hashmap_init_wcapacity(hashmap *hm, size_t capacity){
  if(capacity == 0){
    return;
  }
  hm->capacity = capacity;
  hm->count = 0;
  hm->tombstone = 0;
  hm->he = calloc(hm->capacity, sizeof(hashmap_entry));
}

extern void hashmap_rehashing(hashmap *hm){
  hashmap_entry *he_old = hm->he;
  int capacity_old = hm->capacity;

  hm->capacity *= 2;
  hm->he = calloc(hm->capacity, sizeof(hashmap_entry));
  hm->count = 0;
  hm->tombstone = 0;

  int index;

  for(int i = 0; i < capacity_old; i++){
    if(he_old[i].key != NULL && he_old[i].key != (char *)-1){

      index = get_index(he_old[i].hash, hm->capacity);

      while(hm->he[index].key != NULL){
        index++;
        index = (index) & (hm->capacity - 1);
      }

      hm->he[index].key = he_old[i].key;
      hm->he[index].value = he_old[i].value;
      hm->he[index].hash = he_old[i].hash;
      hm->count++;
    }
  }
  free(he_old);
}

extern int hashmap_set(hashmap *hm, const char *key, void *value){

  if (!hm || !key) return 1;

  if(hm->count >= hm->capacity * 0.75){
    hashmap_rehashing(hm);
  }

  uint32_t hash = fnv1a_32(key);
  size_t index = get_index(hash, hm->capacity);
  size_t index_tombstone;
  int found_tombstone = 0;
  int loops = 0;

  while(loops < hm->capacity){
    index = (index) & (hm->capacity - 1);

    if(hm->he[index].key == NULL ){

      if (found_tombstone) {
        index = index_tombstone;
      }

      hm->he[index].key = strdup(key);
      if(hm->he[index].key == NULL){
        return 1;
      }
      hm->he[index].value = value;
      hm->he[index].hash = hash;
      hm->count++;
      return 0;
    }
    if(hm->he[index].key == (char *)-1){
      if (!found_tombstone) {
        index_tombstone = index;
        found_tombstone = 1;
      }
    }else if(hm->he[index].hash == hash){
      if(strcmp(hm->he[index].key, key) == 0){
        hm->he[index].value = value;
        return 0;
      }
    }

    loops++;
    index++;
  }

  if(found_tombstone){
    hm->he[index_tombstone].key = strdup(key);
    hm->he[index_tombstone].value = value;
    hm->he[index_tombstone].hash = hash;
    if(hm->he[index_tombstone].key == NULL){
      return 1;
    }
    hm->count++;
    return 0;
  }

  return 1; 
}

extern void *hashmap_get(hashmap *hm, const char *key){

  if (!hm || !key) return NULL;

  uint32_t hash = fnv1a_32(key);
  size_t index = get_index(hash, hm->capacity);
  int loops = 0;

  while(loops < hm->capacity){
    index = (index) & (hm->capacity - 1);
    if(hm->he[index].key == NULL){
      return NULL;
    }
    if(hm->he[index].key != NULL && hm->he[index].key != (char *)-1 && hm->he[index].hash == hash){
      if(strcmp(hm->he[index].key, key) == 0){
        return hm->he[index].value;
      }
    }
    loops++;
    index++;
  }

  return NULL;
}

extern int hashmap_has(hashmap *hm, const char *key){

  if (!hm || !key) return 0;

  uint32_t hash = fnv1a_32(key);
  size_t index = get_index(hash, hm->capacity);
  int loops = 0;

  while(loops < hm->capacity){
    index = (index) & (hm->capacity - 1);
    if(hm->he[index].key == NULL){
      return 0;
    }
    if(hm->he[index].key != NULL && hm->he[index].key != (char *)-1 && hm->he[index].hash == hash){
      if(strcmp(hm->he[index].key, key) == 0){
        return 1;
      }
    }
    loops++;
    index++;
  }

  return 0;
}

extern int hashmap_remove(hashmap *hm, const char *key){

  if (!hm || !key) return 1;

  uint32_t hash = fnv1a_32(key);
  size_t index = get_index(hash, hm->capacity);
  int loops = 0;

  while(loops < hm->capacity){
    index = (index) & (hm->capacity - 1);
    if(hm->he[index].key == NULL){
      return 1;
    }
    if(hm->he[index].key != NULL && hm->he[index].key != (char *)-1 && hm->he[index].hash == hash){
      if(strcmp(hm->he[index].key, key) == 0){
        free(hm->he[index].key);
        hm->he[index].key = (char *)-1;
        hm->he[index].value = NULL;
        hm->count--;
        hm->tombstone++;
        return 0;
      }
    }
    loops++;
    index++;
  }

  return 1;
}

extern void hashmap_destroy(hashmap *hm){
  
  if (!hm || !hm->he) return;

  for(int i = 0; i < hm->capacity; i++){
    if(hm->he[i].key != NULL && hm->he[i].key != (char *)-1){
      free(hm->he[i].key);
    }
  }
  free(hm->he);
  hm->he = NULL;
  hm->capacity = 0;
  hm->count = 0;
  hm->tombstone = 0;
}

extern int hashmap_next(hashmap *hm, int *iter, char **key, void **value){
    
  if(!hm) return 0;

  for(int i = (*iter); i < hm->capacity; i++){
    if(hm->he[i].key != NULL && hm->he[i].key != (char *)-1){
      *key = hm->he[i].key;
      *value = hm->he[i].value;
      (* iter) = i + 1;
      return 1;
    }
  }
  return 0;
}

extern void hashmap_foreach(const hashmap *hm, hashmap_callback cb, void *user_data) {
    if (!hm || !cb) return;

    for (int i = 0; i < hm->capacity; i++) {
        if (hm->he[i].key != NULL && hm->he[i].key != (char *)-1) {
            cb(hm->he[i].key, hm->he[i].value, user_data);
        }
    }
}
