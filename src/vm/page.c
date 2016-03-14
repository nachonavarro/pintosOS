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

struct spt
get_spt_entry(struct hash *table, void *address)
{
	struct spt *entry;
	entry->vaddr = address;
	struct hash_elem *hash_elem = hash_find(table, entry->elem);
	return hash_entry(hash_elem, struct spt, elem);

}

// TODO: Implement supplemental page table methods
void
load_from_disk(struct spt *spt_entry)
{
	struct thread *cur = thread_current();
	void *page = alloc_frame(spt_entry->vaddr);
	swap_out(spt_entry->vaddr, spt_entry->swap_slot);
	hash_delete (cur->supp_pt, &spt_entry->elem);
}

void
load_file(struct spt *entry)
{

}

void
load_mmf(struct spt *entry)
{

}



