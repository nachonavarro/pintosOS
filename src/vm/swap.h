#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/vaddr.h"
#include "devices/block.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)
#define BITMAP_START_INDEX 0
#define NUM_OF_SLOTS_TO_SWAP 1

void swap_init(void);
void swap_out(void *, size_t);
size_t swap_in(void *);

#endif /* vm/swap.h */
