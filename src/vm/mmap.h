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
  void *uaddr; /* Start address that file is mapped to. */
};

void mmap_table_init(void);
struct mmap_mapping *mmap_table_insert(struct mmap_mapping *new);
struct mmap_mapping *mmap_mapping_lookup(const mapid_t mapid);
void mmap_mapping_delete(const mapid_t mapid);
void mmap_table_flush(void);

#endif /* vm/mmap.h */
