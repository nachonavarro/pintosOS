#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"

static struct lock spt_lock;

static unsigned generate_hash(const struct hash_elem *e, void *aux UNUSED);
static bool compare_less_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void hash_free_elem(struct hash_elem *e, void *aux);

/* Initialises the supplemental page table and the spt_lock */
void
spt_init (struct hash *spt)
{
  hash_init(spt, generate_hash, compare_less_hash, 0);
  lock_init(&spt_lock);
}

/* Function which generates a hashkey, given a hash_elem */
static unsigned 
generate_hash (const struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *supp_page_table = hash_entry(e, struct spt_entry, elem);
  return (unsigned) supp_page_table->vaddr;
}

/* Function which compares 2 hash_elems by comparing their hashkey */
static bool
compare_less_hash (const struct hash_elem *a, 
                   const struct hash_elem *b, 
                   void *aux UNUSED)
{
  struct spt_entry *supp_page_table_a = hash_entry(a, struct spt_entry, elem);
  uint32_t vaddr_a = (uint32_t) supp_page_table_a->vaddr;

  struct spt_entry *supp_page_table_b = hash_entry(b, struct spt_entry, elem);
  uint32_t vaddr_b = (uint32_t) supp_page_table_b->vaddr;

  return vaddr_a <= vaddr_b;
}

/* Inserts a spt_entry ENTRY into the supplemental_page_table SPT.
   If the entry already exists, replaces it with its new value. */
void
spt_insert(struct hash *spt, struct spt_entry *entry)
{
  struct hash_elem *elem;
  lock_acquire(&spt_lock);
  elem = hash_insert(spt, &entry->elem);
  lock_release(&spt_lock);

  if (elem != NULL)
    lock_acquire(&spt_lock);
    hash_replace(spt, &entry->elem);
    lock_release(&spt_lock);
}

/* Returns the spt_entry from the supplemental_page_table given the virtual address of the page */
struct spt_entry*
get_spt_entry(struct hash *table, void *address)
{

  // TODO: Replace palloc_get_page with frame_alloc?

  void *new_page = palloc_get_page(0);
	struct spt_entry *entry = new_page;

	entry->vaddr = address;
  lock_acquire(&spt_lock);
	struct hash_elem *hash_elem = hash_find(table, &entry->elem);
  lock_release(&spt_lock);

	palloc_free_page(new_page);

  if (hash_elem == NULL)
    return NULL;

	return hash_entry(hash_elem, struct spt_entry, elem);
}


void
load_from_disk(struct spt_entry *spt_entry)
{
	struct thread *cur = thread_current();
	void *page = frame_alloc(PAL_USER, spt_entry->vaddr);
	swap_out(page, spt_entry->swap_slot);
  lock_acquire(&spt_lock);
	hash_delete (&cur->supp_pt, &spt_entry->elem);
  lock_release(&spt_lock);
}

void
load_file(struct spt_entry *entry)
{
    entry->file = true;
    return;
}

void
load_mmf(struct spt_entry *entry)
{
    entry->mmf = true;
    return;
}

/* Frees each spt_entry of the hashmap and destroys it. */
void 
spt_destroy (struct hash *hashmap)
{
  lock_acquire(&spt_lock);
  hash_destroy(hashmap, hash_free_elem);
  lock_release(&spt_lock);
}

/* Frees an spt_entry, used in spt_destroy. */
static void 
hash_free_elem (struct hash_elem *e, void *aux)
{
  struct spt_entry *entry = hash_entry(e, struct spt_entry, elem);
  palloc_free_page(entry);
}



