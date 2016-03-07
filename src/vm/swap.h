#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/vaddr.h"
#include "devices/block.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_out(void *);
void swap_int(void *);

#endif /* vm/swap.h */
