#include "vm/swap.h"
#include "devices/block.h"

struct block *swap_space;
static struct bitmap *swap_bitmap;
static int pages_in_swap_space;

void
swap_init()
{
    swap_space = block_get_role(BLOCK_SWAP);
    pages_in_swap_space = block_size(swap_space) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(pages_in_swap_space);
}

void
swap_in(void *page_start)
{
    /* Find a free slot and set it to occupied.*/
    int free_slot_index =
            bitmap_scan_and_flip(swap_bitmap, BITMAP_START_INDEX,
                                    NUM_OF_SLOTS_TO_SWAP, false);
    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_write(swap_space, SECTORS_PER_PAGE * free_slot_index + i,
                                                            page_start + i);
        }

    int i = 0;

}


//TODO: Billy, the frame table will need to supply the slot it needs from disk.
void
swap_out(void *page_start, size_t swap_slot)
{

    int free_slot_index = bitmap_flip(swap_bitmap, swap_slot);
    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_read(swap_space, i, SECTORS_PER_PAGE * free_slot_index + i,
                                                              page_start + i);
        }
}
