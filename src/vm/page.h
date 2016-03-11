#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/hash.h"

struct spt {
    struct hash hash_table;
    struct hash_iterator iterator;
    // TODO: Implement struct
};

void spt_init(void);
uint32_t spt_lookup(uint32_t fault_address);
void spt_insert(void);
void spt_remove(void);

// TODO: Implement spt methods


#endif /* vm/page.h */