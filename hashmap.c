#include "hashmap.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Round up to the next power of two
//
// Trick: spread the highest set bit to the right, then add 1.
// Example: n = 200 (binary 11001000)
//   n--           -> 199 = 11000111
//   n |= n >> 1   -> 11100111
//   n |= n >> 2   -> 11111111
//   n |= n >> 4   -> 11111111
//   n |= n >> 8   -> 11111111
//   n |= n >> 16  -> 11111111 (and >>32 on 64-bit)
// At this point, all bits below the top bit are set.
// Adding 1 gives 100000000 = 256, the next power of two.
//
// If n was already a power of two, the initial n-- ensures we don’t jump to the next one.
size_t next_pow2(size_t n) {
  if (n == 0)
    return 1;

  n--; // if already power of two, stay there
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
  n |= n >> 32; // for 64-bit size_t
#endif
  return n + 1;
}
uint64_t hash_cstr_fnv1a(const char *s) {
  uint64_t h = 1469598103934665603ULL; // offset basis
  while (*s) {
    h ^= (unsigned char)(*s++);
    h *= 1099511628211ULL; // FNV prime
  }
  return h;
}

Slot *make_slots(size_t count) {
  if (count && count > SIZE_MAX / sizeof(Slot)) {
    return NULL;
  }
  return calloc(count, sizeof(Slot));
}

Hashmap *hm_create(size_t initial_capacity) {

  Hashmap *buf = calloc(1, sizeof(*buf));
  if (!buf) {
    return NULL;
  }

  buf->capacity = next_pow2(initial_capacity);
  buf->slots = make_slots(buf->capacity);
  if (!buf->slots) {
    free(buf);
    return NULL;
  }

  return buf;
}

void free_slots(Slot *slots, size_t capacity) {
  if (!slots) {
    return;
  }

  for (size_t i = 0; i < capacity; i++) {
    if (slots[i].state == SLOT_OCCUPIED) {
      free((void *)slots[i].key);
    }
  }
  free(slots);
}

void hm_free(Hashmap *map) {
  if (!map) {
    return;
  }
  free_slots(map->slots, map->capacity);
  free(map);
}

int slot_insert(Slot *slots, const char *key, void *value, size_t *size, size_t cap) {
  if (!slots || cap == 0 || !key) {
    return 1;
  }

  uint64_t h = hash_cstr_fnv1a(key);
  size_t idx = h & (cap - 1);

  size_t i = idx;
  Slot *slot = &slots[idx];
  size_t steps = 0;

  while (slot->state != SLOT_EMPTY && steps < cap) {
    slot = &slots[(i++) & (cap - 1)];
    steps++;
  }

  if (steps >= cap) {
    return 2;
  }

  slot->key = key;
  slot->value = value;
  slot->state = SLOT_OCCUPIED;
  (*size)++;

  return 0;
}

int hm_resize(Hashmap *map) {
  if (!map) {
    return 1;
  }

  size_t old_cap = map->capacity;
  Slot *old_slots = map->slots;

  size_t cap = map->capacity * 2;
  if (cap < map->capacity) {
    return 2;
  }
  size_t size = 0;
  Slot *new_slots = make_slots(cap);
  if (new_slots == NULL) {
    return 2;
  }

  for (size_t i = 0; i < old_cap; i++) {
    if (old_slots[i].state == SLOT_OCCUPIED) {
      int result = slot_insert(new_slots, old_slots[i].key, old_slots[i].value, &size, cap);
      if (result != 0) {
        free(new_slots);
        return 3;
      }
    }
  }

  map->capacity = cap;
  map->size = size;
  map->slots = new_slots;
  map->del_count = 0;
  free(old_slots);
  return 0;
}

int hm_insert(Hashmap *map, const char *key, void *value) {
  if (!map || !map->slots || map->capacity == 0 || !key) {
    return 1;
  }

  if (((double)(map->size + 1) / (double)map->capacity) > 0.75 ||
      (map->size > 0 && ((double)map->del_count / (double)map->size) > 0.8)) {
    int result = hm_resize(map);
    if (result != 0) {
      return 1;
    }
  }

  uint64_t h = hash_cstr_fnv1a(key);
  size_t idx = h & (map->capacity - 1);

  size_t i = idx;
  Slot *slot = &map->slots[idx];
  size_t del_i = SIZE_MAX;
  size_t steps = 0;

  while (slot->state != SLOT_EMPTY && steps < map->capacity) {
    if (slot->state == SLOT_DELETED && del_i == SIZE_MAX) {
      del_i = i;
    }

    if (slot->state == SLOT_OCCUPIED && strcmp(slot->key, key) == 0) {
      slot->value = value;
      return 0;
    }
    i = (i + 1) & (map->capacity - 1);
    slot = &map->slots[i];
    steps++;
  }

  if (steps == map->capacity && del_i == SIZE_MAX) {
    return 4;
  }

  if (del_i != SIZE_MAX) {
    slot = &map->slots[del_i];
    map->del_count--;
  }

  slot->key = strdup(key);
  if (slot->key == NULL) {
    return 2;
  }
  slot->value = value;
  slot->state = SLOT_OCCUPIED;
  map->size++;

  return 0;
}

const Slot *hm_search(Hashmap *map, const char *key) {
  if (!map || !map->slots || !key || map->capacity == 0) {
    return NULL;
  }

  size_t idx = hash_cstr_fnv1a(key) & (map->capacity - 1);
  Slot *slot = &map->slots[idx];
  size_t steps = 0;

  while (slot->state != SLOT_EMPTY && steps < map->capacity) {
    if (slot->state == SLOT_OCCUPIED && strcmp(slot->key, key) == 0) {
      return slot;
    }

    idx = (idx + 1) & (map->capacity - 1);
    slot = &map->slots[idx];
    steps++;
  }
  return NULL;
}

int hm_update(Hashmap *map, const char *key, void *value) {
  if (!map || !map->slots || !key || map->capacity == 0) {
    return -1;
  }

  size_t idx = hash_cstr_fnv1a(key) & (map->capacity - 1);
  Slot *slot = &map->slots[idx];
  size_t steps = 0;

  while (slot->state != SLOT_EMPTY && steps < map->capacity) {
    if (slot->state == SLOT_OCCUPIED && strcmp(slot->key, key) == 0) {
      slot->value = value;
      return 0;
    }

    idx = (idx + 1) & (map->capacity - 1);
    slot = &map->slots[idx];
    steps++;
  }
  return -1;
}

int hm_delete(Hashmap *map, const char *key) {
  if (!map || !map->slots || !key || map->capacity == 0) {
    return -1;
  }

  size_t idx = hash_cstr_fnv1a(key) & (map->capacity - 1);
  Slot *slot = &map->slots[idx];
  size_t steps = 0;

  while (slot->state != SLOT_EMPTY && steps < map->capacity) {
    if (slot->state == SLOT_OCCUPIED && strcmp(slot->key, key) == 0) {
      free((void *)slot->key);
      slot->key = NULL;
      slot->value = NULL;
      slot->state = SLOT_DELETED;
      map->del_count++;

      if (map->size > 0)
        map->size--;
      return 0;
    }

    idx = (idx + 1) & (map->capacity - 1);
    slot = &map->slots[idx];
    steps++;
  }
  return -1;
}
