#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"

static struct lock spt_lock;

static unsigned generate_hash(const struct hash_elem *e, void *aux UNUSED);
static bool compare_less_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static void hash_free_elem(struct hash_elem *e, void *aux UNUSED);

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

  if (elem != NULL)
    hash_replace(spt, &entry->elem);

  lock_release(&spt_lock);
}

/* Returns the spt_entry from the supplemental_page_table given the virtual address of the page */
struct spt_entry*
get_spt_entry(struct hash *table, void *address)
{

  // TODO: Replace palloc_get_page with frame_alloc?

	struct spt_entry *entry;

	entry->vaddr = address;
  lock_acquire(&spt_lock);
	struct hash_elem *hash_elem = hash_find(table, &entry->elem);
  lock_release(&spt_lock);

  if (hash_elem == NULL)
    return NULL;

	return hash_entry(hash_elem, struct spt_entry, elem);
}

// TODO: Do we want to delete from hash table after swapping out?
void
load_from_disk(void *page, struct spt_entry *spt_entry)
{
	swap_out(page, spt_entry->swap_slot);
}

void
load_file(void *page, struct spt_entry *entry)
{
    entry->file = true;
    return;
}

void
load_mmf(void *page, struct spt_entry *entry)
{
    entry->mmf = true;
    return;
}

// Loads the data into PAGE from its location in SPT_ENTRY
void load_into_page (void *page, struct spt_entry *spt_entry)
{

  // TODO: Use memcpy from file system, swap slot, or zero the page?

  // If page data is in swap slot, swap out, into the frame
  if (spt_entry->swap) {
    load_from_disk(page, spt_entry);

  // If page data is in file system, load file into frame 
  } else if (spt_entry->file) {
    load_file(page, spt_entry);

  // If page data is in memory mapped files, load into frame
  } else if (spt_entry->mmf) {
    load_mmf(page, spt_entry);

  // If page should be all-zero, fill it with zeroes
  } else {
    // TODO: zero the page?
  }
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
hash_free_elem (struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *entry = hash_entry(e, struct spt_entry, elem);
  palloc_free_page(entry);
}



