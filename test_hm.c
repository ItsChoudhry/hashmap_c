// test_hm.c
#include "hashmap.h" // your header
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int *make_int(int v) {
  int *p = malloc(sizeof *p);
  assert(p);
  *p = v;
  return p;
}

// Free all caller-owned values that are still stored in the map.
// (Keys are owned/freed by the hashmap.)
static void free_remaining_values(Hashmap *hm) {
  for (size_t i = 0; i < hm->capacity; ++i) {
    if (hm->slots[i].state == SLOT_OCCUPIED && hm->slots[i].value) {
      free(hm->slots[i].value);
      hm->slots[i].value = NULL;
    }
  }
}

static void test_basic_insert_get(void) {
  Hashmap *hm = hm_create(16);
  assert(hm);

  assert(hm_insert(hm, "alice", make_int(10)) == 0);
  assert(hm_insert(hm, "bob", make_int(20)) == 0);
  assert(hm_insert(hm, "carol", make_int(30)) == 0);
  assert(hm->size == 3);

  const Slot *s;

  s = hm_search(hm, "alice");
  assert(s && s->value && *(int *)s->value == 10);
  s = hm_search(hm, "bob");
  assert(s && s->value && *(int *)s->value == 20);
  s = hm_search(hm, "carol");
  assert(s && s->value && *(int *)s->value == 30);

  s = hm_search(hm, "nope");
  assert(s == NULL);

  free_remaining_values(hm);
  hm_free(hm);
}

static void test_update(void) {
  Hashmap *hm = hm_create(8);
  assert(hm);

  assert(hm_insert(hm, "x", make_int(1)) == 0);
  // Replace value (caller frees old one)
  const Slot *sx = hm_search(hm, "x");
  assert(sx && sx->value);
  free(sx->value);

  assert(hm_update(hm, "x", make_int(42)) == 0);
  sx = hm_search(hm, "x");
  assert(sx && *(int *)sx->value == 42);
  assert(hm->size == 1); // size unchanged on update

  free_remaining_values(hm);
  hm_free(hm);
}

static void test_delete(void) {
  Hashmap *hm = hm_create(8);
  assert(hm);

  assert(hm_insert(hm, "k1", make_int(11)) == 0);
  assert(hm_insert(hm, "k2", make_int(22)) == 0);
  assert(hm->size == 2);

  // Delete k1 (caller frees value first)
  const Slot *s = hm_search(hm, "k1");
  assert(s && s->value);
  free(s->value);
  assert(hm_delete(hm, "k1") == 0);
  assert(hm->size == 1);

  // k2 still present
  s = hm_search(hm, "k2");
  assert(s && *(int *)s->value == 22);

  // k1 is gone
  s = hm_search(hm, "k1");
  assert(s == NULL);

  free_remaining_values(hm);
  hm_free(hm);
}

static void test_resize_and_lookup(void) {
  Hashmap *hm = hm_create(16);
  assert(hm);
  size_t N = 2000; // force multiple resizes

  // Insert N pairs
  for (size_t i = 0; i < N; ++i) {
    char key[32];
    snprintf(key, sizeof key, "thing_%zu", i);
    assert(hm_insert(hm, key, make_int((int)i)) == 0);
  }
  assert(hm->size == N);

  // Spot-check some values
  for (size_t i = 0; i < N; i += 199) {
    char key[32];
    snprintf(key, sizeof key, "thing_%zu", i);
    const Slot *s = hm_search(hm, key);
    assert(s && s->value && *(int *)s->value == (int)i);
  }

  // Clean up all values
  free_remaining_values(hm);
  hm_free(hm);
}

static void test_tombstone_reuse(void) {
  Hashmap *hm = hm_create(16);
  assert(hm);

  // Insert a handful
  assert(hm_insert(hm, "a", make_int(1)) == 0);
  assert(hm_insert(hm, "b", make_int(2)) == 0);
  assert(hm_insert(hm, "c", make_int(3)) == 0);
  size_t size_before = hm->size;

  // Delete "b"
  const Slot *sb = hm_search(hm, "b");
  assert(sb && sb->value);
  free(sb->value);
  assert(hm_delete(hm, "b") == 0);
  assert(hm->size == size_before - 1);

  // Reinsert a new key; implementation should prefer the tombstone
  assert(hm_insert(hm, "d", make_int(4)) == 0);
  assert(hm->size == size_before); // back to original count

  // All present as expected
  const Slot *sa = hm_search(hm, "a");
  assert(sa && *(int *)sa->value == 1);
  const Slot *sc = hm_search(hm, "c");
  assert(sc && *(int *)sc->value == 3);
  const Slot *sd = hm_search(hm, "d");
  assert(sd && *(int *)sd->value == 4);
  assert(hm_search(hm, "b") == NULL);

  free_remaining_values(hm);
  hm_free(hm);
}

int main(void) {
  test_basic_insert_get();
  test_update();
  test_delete();
  test_resize_and_lookup();
  test_tombstone_reuse();
  puts("All tests passed ✅");
  return 0;
}
