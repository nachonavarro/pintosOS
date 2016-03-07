#include "vm/swap.h"
#include "devices/block.h"

struct block *swap_block;

void
swap_init()
{
    swap_block = block_get_role(BLOCK_SWAP);
}

void
swap_in(void *page_start)
{
    int sectors_in_block = swap_block->size;
    int i;
    for (i = 0; i < sectors_in_block; i++)
        {
            block_write(swap_block, i, page_start + i);
        }
}

void
swap_out(void *page_start)
{
    int sectors_in_block = swap_block->size;
    int i;
    for (i = 0; i < sectors_in_block; i++)
        {
            block_read(swap_block, i, page_start + i);
        }
}
