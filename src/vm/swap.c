#include "vm/swap.h"
#include "devices/block.h"

struct block *swap_block;
static struct bitmap *swap_bitmap;
static int pages_in_swap_space;

void
swap_init()
{
    swap_block = block_get_role(BLOCK_SWAP);
    pages_in_swap_space = block_size(swap_block) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(pages_in_swap_space);
}

void
swap_in(void *page_start)
{

    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_write(swap_block, i, page_start + i);
        }
}

void
swap_out(void *page_start)
{
    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_read(swap_block, i, page_start + i);
        }
}
