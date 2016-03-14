#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

struct spt {
    size_t swap_slot;
	uint32_t vaddr;
    uint32_t starting_address;
    bool swap;
    bool file;
    bool mmf;
    struct hash_elem elem;

};

void spt_init(void);
uint32_t spt_lookup(uint32_t fault_address);
void spt_insert(void);
struct spt get_spt_entry(struct hash *table, void *address);
void load_from_disk(struct spt *entry);
void load_file(struct spt *entry);
void load_mmf(struct spt *entry);
// void spt_remove(void);
// Is removal needed?

// TODO: Implement spt methods


#endif /* vm/page.h */
