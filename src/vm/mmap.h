#ifndef VM_MMAP_H
#define VM_MMAP_H

#include "userprog/syscall.h"
#include "lib/kernel/hash.h"

struct mmap_mapping {
  struct hash_elem hash_elem;
  mapid_t mapid; /* Uniquely identifies the mapping (within the process). The
                    hash table mmap_table will take mapid as a key, and the
                    struct mmap_mapping it is in will be the value. */
//  int size; /* Size of file in bytes. If size is not a multiple of PGSIZE,
//               some bytes in the final mapped page 'stick out' beyond end of
//               file. */
  int num_pages; /* Number of pages that this file will take up when mapped.
                    If end file is just 1 byte into a page, that whole page is
                    needed. */
  //TODO: Not sure whether to have size or end_uaddr
  void *start_uaddr; /* Start address that file is mapped to. */
  void *end_uaddr; /* End address that file is mapped to. */
  //TODO: For eviction, having a file descriptor here may be better?
  //      Maybe we need both
  struct file *file; /* File that is mapped. Not the same struct as in
                        another mmap_mapping for same file, as
                        file_reopen() is used. */
};

bool mmap_table_insert(struct hash *mmap_table, void *start_uaddr,
    void *end_uaddr, int num_pages, mapid_t mapid, struct file* file);
struct mmap_mapping *mmap_mapping_lookup(struct hash *mmap_table,
                                                    const mapid_t mapid);
void mmap_mapping_delete(struct hash *mmap_table, struct mmap_mapping *mmap);
unsigned mapid_hash(const struct hash_elem *, void *);
bool mapid_less(const struct hash_elem *, const struct hash_elem *, void *);
void destroy_mmap_table(struct hash *mmap_table);

#endif /* vm/mmap.h */
