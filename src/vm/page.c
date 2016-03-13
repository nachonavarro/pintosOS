#include "vm/page.h"

void
spt_init (struct hash *spt)
{
  hash_init(spt, generate_hash, compare_less_hash, aux);
}

static unsigned 
generate_hash (const struct hash_elem *e, void *aux)
{
  struct spt *supp_page_table = hash_entry(e, struct spt, elem);
  return (unsigned) supp_page_table->vaddr;
}

static bool
compare_less_hash (const struct hash_elem *a, 
                   const struct hash_elem *b, 
                   void *aux)
{
  struct spt *supp_page_table_a = hash_entry(a, struct spt, elem);
  uint32_t vaddr_a = supp_page_table_a->vaddr;

  struct spt *supp_page_table_b = hash_entry(b, struct spt, elem);
  uint32_t vaddr_b = supp_page_table_b->vaddr;

  return vaddr_a <= vaddr_b;
}

// TODO: Implement supplemental page table methods

