#include <debug.h>
#include <stddef.h>
#include "vm/mmap.h"
#include "threads/synch.h"
#include "threads/malloc.h"

//TODO: Don't think we need to flush or reset next_mapid anymore, as each
//      process has a mmap_table and next_mapid, and flushing and resetting
//      next_mapid is only necessary when process exits.

// TODO: Define this function?
// static void free_using_hash_elem(struct hash_elem *, void*);

/* Malloc's space for a struct mmap_mapping and sets all of its members,
   before inserting it into mmap_table. Returns false if not enough
   space to malloc. */
bool
mmap_table_insert(struct hash *mmap_table, void *start_uaddr, void *end_uaddr,
    int num_pages, mapid_t mapid, struct file *file) {
  /* Freed in mmap_mapping_delete(). */
  struct mmap_mapping *mmap = malloc(sizeof(struct mmap_mapping));
  if (mmap == NULL) {
    return false;
  }
  mmap->start_uaddr = start_uaddr;
  mmap->end_uaddr = end_uaddr;
  mmap->num_pages = num_pages;
  mmap->mapid = mapid;
  mmap->file = file;
  hash_insert(mmap_table, &mmap->hash_elem);
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

/* Remove the mapping MMAP from MMAP_TABLE. Should guarantee that MMAP is in
   MMAP_TABLE before calling this, by calling mmap_mapping_lookup first. */
void
mmap_mapping_delete(struct hash *mmap_table, struct mmap_mapping *mmap) {
  hash_delete(mmap_table, &mmap->hash_elem);
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
