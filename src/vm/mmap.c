#include <debug.h>
#include <stddef.h>
#include "vm/mmap.h"
#include "threads/synch.h"
#include "threads/malloc.h"

//TODO: Don't think we need to flush or reset next_mapid anymore, as each
//      process has a mmap_table and next_mapid, and flushing and resetting
//      next_mapid is only necessary when process exits.

static void free_using_hash_elem(struct hash_elem *, void*);

/* Malloc's space for a struct mmap_mapping and sets all of its members,
   before inserting it into mmap_table. Returns false if not enough
   space to malloc. */
bool
mmap_table_insert(struct hash *mmap_table, void *uaddr, int size, mapid_t mapid, struct file *file) {
  /* Freed in mmap_mapping_delete(). */
  struct mmap_mapping *mmap = malloc(sizeof(struct mmap_mapping));
  if (mmap == NULL) {
    return false;
  }
  mmap->uaddr = uaddr;
  mmap->size = size;
  mmap->mapid = mapid;
  mmap->file = file;
//  lock_acquire(&mmap_table_lock);
  hash_insert(mmap_table, &mmap->hash_elem);
//  lock_release(&mmap_table_lock);
  return true;
}

/* Return the 'struct mmap_mapping *' corresponding to MAPID. Returns null if
   no such elements exists in mmap_table. */
struct mmap_mapping *
mmap_mapping_lookup(struct hash *mmap_table, const mapid_t mapid) {
  /* It is only ok to allocate this struct mmap_mapping as a local variable
     as it is small. */
  struct mmap_mapping m;
  struct hash_elem *e;

  m.mapid = mapid;
  e = hash_find(mmap_table, &m.hash_elem);
  return e != NULL ? hash_entry(e, struct mmap_mapping, hash_elem) : NULL;
}

/* Remove the mapping in mmap_table from MAPID to the corresponding struct
   mmap_mapping. */
void
mmap_mapping_delete(struct hash *mmap_table, const mapid_t mapid) {
  struct mmap_mapping *mmap = mmap_mapping_lookup(mmap_table, mapid);
  /* If no element with mapid_t MAPID is in mmap_table, then simply return. */
  if (mmap == NULL) {
    return;
  }
//  lock_acquire(&mmap_table_lock);
  hash_delete(mmap_table, &mmap->hash_elem);
//  lock_release(&mmap_table_lock);
  /* Free the struct mmap_mapping allocated using malloc in
     mmap_table_insert(). */
  free(mmap);
}

unsigned
mapid_hash(const struct hash_elem *mmap_map_, void *aux UNUSED) {
  const struct mmap_mapping *mmap_map = hash_entry(mmap_map_, struct mmap_mapping, hash_elem);
//  TODO: Not 100% sure which of these hash functions is better.
//        return (unsigned) hash_int((int) (mmap_map->mapid));
  mapid_t mapid = mmap_map->mapid;
  return hash_bytes(&mapid, sizeof(mapid));
}

bool
mapid_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED) {
  const struct mmap_mapping *a = hash_entry(a_, struct mmap_mapping, hash_elem);
  const struct mmap_mapping *b = hash_entry(b_, struct mmap_mapping, hash_elem);
  return a->mapid < b->mapid;
}

void
destroy_mmap_table(struct hash *mmap_table) {
  hash_destroy(mmap_table, free_using_hash_elem);
}

/* Destructor hash_action_func to be passed to hash_destroy to free all
   malloc'd struct mmap_mapping's in a mmap_table. */
static void
free_using_hash_elem(struct hash_elem *e, void* aux UNUSED) {
  struct mmap_mapping *mmap = hash_entry(e, struct mmap_mapping, hash_elem);
  free(mmap);
}
