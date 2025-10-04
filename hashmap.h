#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED, SLOT_DELETED } SlotState;

typedef struct {
  const char *key;
  void *value;
  SlotState state;
} Slot;

typedef struct {
  Slot *slots;      // dynamic array of slots
  size_t capacity;  // total slots
  size_t size;      // number of elements stored
  size_t del_count; // number of elements stored
} Hashmap;

// API
Hashmap *hm_create(size_t initial_capacity);
void hm_free(Hashmap *map);

int hm_insert(Hashmap *map, const char *key, void *value);
const Slot *hm_search(Hashmap *map, const char *key);
int hm_delete(Hashmap *map, const char *key);
int hm_update(Hashmap *map, const char *key, void *value);

#endif // HASHMAP_H
