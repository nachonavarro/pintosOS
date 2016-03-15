#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

struct spt_entry {
    size_t swap_slot;
	void  *vaddr;
    uint32_t starting_address;
    bool swap;
    bool file;
    bool mmf;
    struct hash_elem elem;
};

void spt_init(struct hash *spt);
void spt_insert(struct hash *spt, struct spt_entry *entry);
struct spt_entry* get_spt_entry(struct hash *table, void *address);

void load_from_disk(struct spt_entry *entry);
void load_file(struct spt_entry *entry);
void load_mmf(struct spt_entry *entry);


// void spt_remove(void);
// Is removal needed?

// void spt_modify(struct hash *spt, struct spt_entry *entry)
// do we need this? or when we insert an already existing spt_elem, it modifies it to its new value?



#endif /* vm/page.h */
