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

bool
spt_insert_all_zero(void *uaddr)
{
  struct thread *cur = thread_current();
  struct hash_elem *elem;
  struct spt_entry *entry = malloc(sizeof(struct spt_entry));
  lock_acquire(&spt_lock);
  if (entry == NULL) {
    return false;
  }
  entry->info = ALL_ZERO;
  entry->vaddr = uaddr;
  entry->in_memory = false;
  entry->file_info.writable = true;
  elem = hash_insert(&cur->supp_pt, &entry->elem);
  if (elem == NULL) {
    lock_release(&spt_lock);
    return true;
  }
  lock_release(&spt_lock);
  return false;
}

/* Used to insert an FSYS or a MMAP file into the supplementary page table.
   If MMAP bool is true, it is MMAP we are inserting, otherwise it is FSYS. */
bool
spt_insert_file(void *uaddr, struct file *f, size_t size, size_t zeros, size_t offset, bool writable,
            bool mmap, bool executable)
{

  struct thread *cur = thread_current();
  struct hash_elem *elem;
  struct spt_entry *entry = malloc(sizeof(struct spt_entry));
  lock_acquire(&spt_lock);
  if (entry == NULL) {
    lock_release(&spt_lock);
    return false;
  }
  entry->file_info.f = f;
  entry->file_info.offset = offset;
  entry->file_info.size = size;
  entry->file_info.zeros = zeros;
  entry->file_info.writable = writable;
  entry->file_info.executable = executable;
  entry->vaddr = uaddr;
  entry->in_memory = false;

  if (mmap) {
      entry->info = MMAP;
  } else {
      entry->info = FSYS;
  }

  elem = hash_insert(&cur->supp_pt, &entry->elem);
  if (elem == NULL) {
	  lock_release(&spt_lock);
	  return true;
  }
  lock_release(&spt_lock);
  return false;
}

/* Returns the spt_entry from the supplemental_page_table given the virtual
   address of the page */
struct spt_entry*
get_spt_entry(struct hash *table, void *address)
{
  lock_acquire(&spt_lock);
	struct spt_entry entry;
	entry.vaddr = address;
	struct hash_elem *elem = hash_find(table, &entry.elem);
  lock_release(&spt_lock);
	return (elem != NULL) ? hash_entry(elem, struct spt_entry, elem) : NULL;
}

void
load_from_disk(void *page, struct spt_entry *spt_entry)
{
    
  	swap_out(page, spt_entry->swap_slot);
    bool success = install_page(spt_entry->vaddr, page, spt_entry->file_info.writable);
    if (!success) {
        frame_free(page);
    }
}

void
load_file(void *kpage, struct spt_entry *entry)
{
	size_t page_read_bytes = entry->file_info.size;

	/*Same as segment loop in exception.c*/
	size_t bytes_actually_read = file_read_at(entry->file_info.f,
                                  kpage, page_read_bytes, entry->file_info.offset);
	if (bytes_actually_read != page_read_bytes)
		{
		  frame_free(kpage);
		  return;
	  }
	memset(kpage + page_read_bytes, 0, entry->file_info.zeros);
	bool success = install_page(entry->vaddr, kpage, entry->file_info.writable);

  if (!success) {
    frame_free(kpage);
  }
}

/* Loads the data into PAGE from its location in SPT_ENTRY */
void load_into_page (void *page, struct spt_entry *spt_entry)
{

  /* If page data is in swap slot, swap out, into the frame */
  if (spt_entry->info == SWAP) {
    load_from_disk(page, spt_entry);
  /* If page data is in file system, load file into frame */
  } else if (spt_entry->info == FSYS) {
    load_file(page, spt_entry);
  /* If page data is in memory mapped files, load into frame */
  } else if (spt_entry->info == MMAP) {
    load_file(page, spt_entry);
  /* If page should be all-zero, fill it with zeroes */
  } else if (spt_entry->info == ALL_ZERO){
  	memset(page, 0, PGSIZE);
    install_page(spt_entry->vaddr, page, true);
  }
  spt_entry->in_memory = true;
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
  free(entry);
}

/* The heuristic to check if stack should grow. */
bool
should_stack_grow(void *addr, void *esp)
{
    bool heuristic;
    /* Check fault address doesn't pass stack limit growth. */
    heuristic = (uint32_t) (PHYS_BASE - pg_round_down(addr)) <= STACK_LIMIT;
    /* Check fault address is above the limit of the stack minus the permission bytes. */
    heuristic &= addr >= esp - PUSHA_PERMISSION_BYTES;

    return heuristic;
}

void
grow_stack(void *addr)
{
    void *page = frame_alloc(PAL_USER, addr);
    pagedir_set_page(thread_current()->pagedir, pg_round_down(addr), page, true);
}
