#include "vm/swap.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "lib/kernel/bitmap.h"
#include "userprog/pagedir.h"

struct block *swap_space;
struct lock swap_lock;
static struct bitmap *swap_bitmap;
static int pages_in_swap_space;


void
swap_init(void)
{
    swap_space = block_get_role(BLOCK_SWAP);
    pages_in_swap_space = block_size(swap_space) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(pages_in_swap_space);
    lock_init(&swap_lock);
}

size_t
swap_in(void *buf)
{
    /* Find a free slot and set it to occupied.*/
    lock_acquire(&swap_lock);
    size_t free_slot_index =
            bitmap_scan_and_flip(swap_bitmap, BITMAP_START_INDEX,
                                    NUM_OF_SLOTS_TO_SWAP, false);
    lock_release(&swap_lock);
    if (free_slot_index == BITMAP_ERROR)
        {
            PANIC("No swap space available.");
        }

    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_write(swap_space, 
                SECTORS_PER_PAGE * free_slot_index + i, 
                   buf + i * BLOCK_SECTOR_SIZE);
        }
    return free_slot_index;
}


//TODO: Billy, the frame table will need to supply the slot it needs from disk.
void
swap_out(void *buf, size_t swap_slot)
{

    lock_acquire(&swap_lock);
    bitmap_flip(swap_bitmap, swap_slot);
    lock_release(&swap_lock);
    int i;
    for (i = 0; i < SECTORS_PER_PAGE; i++)
        {
            block_read(swap_space, 
                SECTORS_PER_PAGE * swap_slot + i, 
                  buf + i * BLOCK_SECTOR_SIZE);
        }
    free(swap_slot);
}


