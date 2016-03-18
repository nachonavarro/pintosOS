#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"
#include <string.h>


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
  struct spt_entry *entry_a = hash_entry(a, struct spt_entry, elem);
  struct spt_entry *entry_b = hash_entry(b, struct spt_entry, elem);

  return entry_a->vaddr < entry_b->vaddr;
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
}

bool
spt_insert_all_zero(void *uaddr)
{

  struct thread *cur = thread_current();
  struct hash_elem *elem;
  struct spt_entry *entry = malloc(sizeof(struct spt_entry)); // WE NEED TO FREEEE.
  lock_acquire(&spt_lock);
  if (entry == NULL) {
	  return false;
  }
  entry->info = ALL_ZERO;
  entry->vaddr = uaddr;
  elem = hash_insert(&cur->supp_pt, &entry->elem); //Should check null?
  if (elem == NULL) {
	  lock_release(&spt_lock);
	  return true;
  }
  lock_release(&spt_lock);
  return false;
}


bool
spt_insert_file(void *uaddr, struct file *f, size_t size, size_t zeros, size_t offset)
{

  struct thread *cur = thread_current();
  struct hash_elem *elem;
  struct spt_entry *entry = malloc(sizeof(struct spt_entry)); // WE NEED TO FREEEE.
  lock_acquire(&spt_lock);
  if (entry == NULL) {
	  return false;
  }
  entry->file_info.f = f;
  entry->file_info.offset = offset;
  entry->file_info.size = size;
  entry->file_info.zeros = zeros;
  entry->info = FSYS;
  entry->vaddr = uaddr;
  elem = hash_insert(&cur->supp_pt, &entry->elem); //Should check null?
  if (elem == NULL) {
	  lock_release(&spt_lock);
	  return true;
  }
  lock_release(&spt_lock);
  return false;
}

/* Returns the spt_entry from the supplemental_page_table given the virtual address of the page */
struct spt_entry*
get_spt_entry(struct hash *table, void *address)
{
	struct spt_entry entry;
	entry.vaddr = address;
	lock_acquire(&spt_lock);
	struct hash_elem *elem = hash_find(table, &entry.elem);
	lock_release(&spt_lock);

	return hash_entry(elem, struct spt_entry, elem);
}

// TODO: Do we want to delete from hash table after swapping out?
void
load_from_disk(void *page, struct spt_entry *spt_entry)
{
	//struct thread *cur = thread_current();
	swap_out(page, spt_entry->swap_slot);
}

void
load_file(void *kpage, struct spt_entry *entry)
{
	struct thread *cur = thread_current();
	size_t page_read_bytes = entry->file_info.size;

	/*Same as segment loop in exception.c*/
  size_t bytes_actually_read = file_read_at(entry->file_info.f, 
                                  kpage, page_read_bytes, entry->file_info.offset);
	if (bytes_actually_read != page_read_bytes)
		{
		  frame_free(kpage);
		  return;
	  }
	// Should we keep a variable zero in file_info?
	memset(kpage + page_read_bytes, 0, entry->file_info.zeros);
	// Not sure if true should always be set.
	bool success = install_page(entry->vaddr, kpage, true);
  if (!success) {
    frame_free(kpage);
  }


}

void
load_mmf(void *page UNUSED, struct spt_entry *entry UNUSED)
{
    return;
}

// Loads the data into PAGE from its location in SPT_ENTRY
void load_into_page (void *page, struct spt_entry *spt_entry)
{
  // TODO: Use memcpy from file system, swap slot, or zero the page?
  // If page data is in swap slot, swap out, into the frame
  
  if (spt_entry->info == SWAP) {
    load_from_disk(page, spt_entry);

  // If page data is in file system, load file into frame 
  } else if (spt_entry->info == FSYS) {
    load_file(page, spt_entry);

  // If page data is in memory mapped files, load into frame
  } else if (spt_entry->info == MMAP) {
    load_mmf(page, spt_entry);

  // If page should be all-zero, fill it with zeroes
  } else if (spt_entry->info == ALL_ZERO){
  	memset(page, 0, PGSIZE);
    install_page(spt_entry->vaddr, page, true);
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
  frame_free(entry->frame_addr);
  free(entry);
}

/* The heuristic to check if stack should grow. */
bool
should_stack_grow(void *addr, void *esp)
{
    bool heuristic;
    /* Check fault address doesn't pass stack limit growth. */
    heuristic = (PHYS_BASE - pg_round_down(addr) <= STACK_LIMIT);
    /* Check fault address is above the limit of the stack minus the permission bytes. */
    heuristic = addr >= esp - PUSHA_PERMISSION_BYTES;
    return heuristic;
}

void
grow_stack(void *addr)
{
    void *page = frame_alloc(PAL_USER, addr);
    /*I think we need to add it to the page table, but for now I'll just insert it manually.*/
    pagedir_set_page(thread_current()->pagedir, pg_round_down(addr), page, true);
}

void hashtable_debug(void)
{
  struct hash *supplemental_page_table = &thread_current()->supp_pt;

  struct hash_iterator i;
  hash_first (&i, supplemental_page_table);

  while (hash_next (&i))
  {
    struct spt_entry *entry = hash_entry(hash_cur (&i), struct spt_entry, elem);

    char *type = NULL;
    enum page_info status = entry->info;
    if (status == FSYS)
      type = "filesys";
    if (status == MMAP)
      type = "memory-mapped";
    if (status == ALL_ZERO)
      type = "zero";
    if (status == SWAP)
      type = "swap";

    printf ("Vaddr: %p Type: %s\n", entry->vaddr, type);
  }
}
