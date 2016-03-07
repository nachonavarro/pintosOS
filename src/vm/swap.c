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

}

void
swap_out(void *page_start)
{

}
