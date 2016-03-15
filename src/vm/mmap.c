#include <debug.h>
#include <stddef.h>
#include "vm/mmap.h"
#include "threads/synch.h"

//TODO: Use hash_insert(&mmap_table, &mmap_mapping->hash_elem) to insert into
//      mmap_table - do this in a mmap_table_insert()?
//TODO: mmap_table should be flushed (hash_clear(mmap_table, NULL) maybe? Or
//      just iterate and delete) when process exits, so we don't need one for
//      each process
static struct hash mmap_table;
static struct lock mmap_table_lock;
//TODO: Put lock only around hash_insert and hash_delete, shouldn't have to for hash_find or hash_next

static unsigned mapid_hash(const struct hash_elem *, void *);
static bool mapid_less(const struct hash_elem *, const struct hash_elem *, void *);

void mmap_table_init(void) {
  hash_init(&mmap_table, mapid_hash, mapid_less, NULL);
  lock_init(&mmap_table_lock);
}

/* Returns NULL if successfully inserts NEW into mmap_table. If it was already
   in the table, returns the mmap_mapping already in the table. */
struct mmap_mapping *
mmap_table_insert(struct mmap_mapping *new) {
  struct hash_elem *e = hash_insert(&mmap_table, &new->hash_elem);
  return e != NULL ? hash_entry(e, struct mmap_mapping, hash_elem) : NULL;
}

/* Return the 'struct mmap_mapping *' corresponding to MAPID. */
struct mmap_mapping *
mmap_mapping_lookup(const mapid_t mapid) {
  /* It is only ok to allocate this struct mmap_mapping as a local variable
     as it is small. */
  struct mmap_mapping m;
  struct hash_elem *e;

  m.mapid = mapid;
  e = hash_find(&mmap_table, &m.hash_elem);
  return e != NULL ? hash_entry(e, struct mmap_mapping, hash_elem) : NULL;
}

/* Remove the mapping in mmap_table from MAPID to the corresponding struct
   mmap_mapping. */
void
mmap_mapping_delete(const mapid_t mapid) {
  /* It is only ok to allocate this struct mmap_mapping as a local variable
     as it is small. */
  struct mmap_mapping m;
  struct hash_elem *e;

  m.mapid = mapid;
  e = hash_find(&mmap_table, &m.hash_elem);
  lock_acquire(&mmap_table_lock);
  hash_delete(&mmap_table, e);
  lock_release(&mmap_table_lock);
  //TODO: Do we need to free the struct mmap_mapping??? (i.e. will we lose
  //      reference to it causing a memory leak?) If so, just use
  //      mmap_mapping_lookup here, then hash_delete the hash_elem, then free.
  //      (In this case, remember to check for NULL returned from lookup!)
}

/* To be called when a process exits - all owned mapped files should be
   flushed. */
void
mmap_table_flush(void) {
  hash_clear(&mmap_table, NULL);
}

static unsigned
mapid_hash(const struct hash_elem *mmap_map_, void *aux UNUSED) {
  const struct mmap_mapping *mmap_map = hash_entry(mmap_map_, struct mmap_mapping, hash_elem);
//  TODO: Not 100% sure which of these hash functions is better.
//return (unsigned) hash_int((int) (mmap_map->mapid));
  mapid_t mapid = mmap_map->mapid;
  return hash_bytes(&mapid, sizeof(mapid));
}

static bool
mapid_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct mmap_mapping *a = hash_entry(a_, struct mmap_mapping, hash_elem);
  const struct mmap_mapping *b = hash_entry(b_, struct mmap_mapping, hash_elem);
  return a->mapid < b->mapid;
}

