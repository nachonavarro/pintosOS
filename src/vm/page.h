#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include <stdio.h>

#define MEGABYTE (1 << 20)
#define STACK_LIMIT (8 * MEGABYTE)
#define PUSHA_PERMISSION_BYTES 32

enum page_info {
	ALL_ZERO,
	SWAP,
	FSYS,
	MMAP
};

struct file_info {
	struct file *f;
	size_t offset;
	size_t size;
};


struct spt_entry {
	void   *vaddr;
    void   *frame_addr;
    size_t swap_slot;
    enum page_info info;
    struct file_info file_info;
    struct hash_elem elem;
};

void spt_init(struct hash *spt);
void spt_insert(struct hash *spt, struct spt_entry *entry);
bool spt_insert_file(void *uaddr, struct file *f, size_t size, size_t offset);
bool spt_insert_all_zero(void *uaddr);
struct spt_entry* get_spt_entry(struct hash *table, void *address);
void spt_destroy(struct hash *hashmap);

void load_into_page(void *page, struct spt_entry *entry);
void load_from_disk(void *page, struct spt_entry *entry);
void load_file(void *page, struct spt_entry *entry);
void load_mmf(void *page, struct spt_entry *entry);

bool should_stack_grow(void *uaddr, void *esp);
void grow_stack(void *addr);
void hashtable_debug(void);


// void spt_modify(struct hash *spt, struct spt_entry *entry)
// do we need this? or when we insert an already existing spt_elem, it modifies it to its new value?



#endif /* vm/page.h */
