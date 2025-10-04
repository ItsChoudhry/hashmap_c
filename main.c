#include "hashmap.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
  Hashmap *hm = hm_create(200);

  size_t n = hm->capacity;
  for (size_t i = 0; i < n; i++) {
    int *val = malloc(sizeof *val);
    *val = i;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "thing_%zu", i);

    hm_insert(hm, buffer, val);
  }

  for (size_t i = 0; i < 10; i++) {
    if (hm->slots[i].state == SLOT_OCCUPIED) {
      printf("%s: %d\n", hm->slots[i].key, *(int *)hm->slots[i].value);
    }
  }

  const Slot *s = hm_search(hm, "thing_0");
  if (s) {
    int *val = (int *)s->value; // cast back to int*
    if (val) {
      printf("%s => %d\n", s->key, *val);
    } else {
      printf("found but value is NULL\n");
    }
  } else {
    printf("not found in hashmap\n");
  }
}
