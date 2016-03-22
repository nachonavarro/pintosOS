#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include <stdio.h>

#define MEGABYTE (1 << 20)
#define STACK_LIMIT (8 * MEGABYTE)
#define PUSHA_PERMISSION_BYTES 32

enum page_info {
	SWAP,
	FSYS,
	MMAP,
	ALL_ZERO
};

struct file_info {
	struct file *f;
	size_t offset;
	size_t size;
	size_t zeros;
	size_t writable;
	bool executable;
};


struct spt_entry {
	void   *vaddr;
  void   *frame_addr;
  size_t swap_slot;
  enum page_info info;
  struct file_info file_info;
  struct hash_elem elem;
  bool in_memory;
};

void spt_init(struct hash *spt);
bool spt_insert_file(void *uaddr, struct file *f, size_t size, size_t zeros,
                     size_t offset, bool writable, bool mmap, bool executable);
bool spt_insert_all_zero(void *uaddr);
struct spt_entry* get_spt_entry(struct hash *table, void *address);
void spt_destroy(struct hash *hashmap);

void load_into_page(void *page, struct spt_entry *entry);
void load_from_disk(void *page, struct spt_entry *entry);
void load_file(void *page, struct spt_entry *entry);
bool install_page(void *upage, void *kpage, bool writable);

bool should_stack_grow(void *uaddr, void *esp);
void grow_stack(void *addr);

#endif /* vm/page.h */
