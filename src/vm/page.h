#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"
#include "threads/palloc.h"

struct spt_entry {
    size_t swap_slot;
	uint32_t vaddr;
    uint32_t starting_address;
    bool swap;
    bool file;
    bool mmf;
    struct hash_elem elem;
};

void spt_init(struct hash *spt);
void spt_insert(struct hash *spt, struct spt_entry *entry);
struct spt_entry* get_spt_entry(struct hash *table, void *address);


// Define following methods in exception.c rather than in page.c?

// void load_from_disk(struct spt_entry *entry);
// void load_file(struct spt_entry *entry);
// void load_mmf(struct spt_entry *entry);


// void spt_remove(void);
// Is removal needed?

// TODO: Implement spt methods


#endif /* vm/page.h */
