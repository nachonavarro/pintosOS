#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "userprog/syscall.h"
#include "lib/kernel/hash.h"

struct mmap_mapping {
  struct hash_elem hash_elem;
  mapid_t mapid; /* Uniquely identifies the mapping (within the process). The
                    hash table mmap_table will take mapid as a key, and the
                    struct mmap_mapping it is in will be the value. */
  int size; /* Size of file in bytes. If size is not a multiple of PGSIZE,
               some bytes in the final mapped page 'stick out' beyond end of
               file. */
  //TODO: Not sure whether to have size or end_uaddr
  void *uaddr; /* Start address that file is mapped to. */
};

bool mmap_table_insert(struct hash *mmap_table, void *uaddr, int size);
struct mmap_mapping *mmap_mapping_lookup(struct hash *mmap_table, const mapid_t mapid);
void mmap_mapping_delete(struct hash *mmap_table, const mapid_t mapid);
unsigned mapid_hash(const struct hash_elem *, void *);
bool mapid_less(const struct hash_elem *, const struct hash_elem *, void *);
void destroy_mmap_table(struct hash *mmap_table);

#endif /* vm/mmap.h */
