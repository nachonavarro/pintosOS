#include <stdbool.h>
#include "vm/frame.h"
#include "lib/random.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "swap.h"
#include "userprog/pagedir.h"

static struct list frame_table;
static struct lock frame_table_lock;

static bool add_frame(void *frame, void *upage);
static void remove_frame(void *frame);
void debug_frame(void);

/* Initialise the actual frame table itself, along with any locks required in
   accessing the frame table. */
void
frame_table_init(void) {
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}

/* Called instead of palloc_get_page() when allocating a user page.
   Gets a page and then adds a frame to the frame table that points to
   that page. Calls a function to handle eviction if the frame table is full.
   Returns the page returned from palloc_get_page(), or the return value of
   evict(). Panics if no frame can evicted without allocating a swap slot, and
   swap slot is full. */
void *
frame_alloc(enum palloc_flags flags, void *upage) {
    /* frame_alloc() must only be called when allocating a user page. */
    ASSERT((flags & PAL_USER) != 0);

  /* A frame is just a page sized region of physical memory, accessed through
     kernel virtual memory (returned from palloc_get_page()) */
  void *frame = palloc_get_page(flags);

  /* If frame table is full, we need to evict a frame, and return the evicted
     frame (where old contents have been evicted and new contents have been
     put in the evicted frame). */
  if (frame == NULL) {
    frame = evict(upage);
    /* evict() will return NULL if no frame can be evicted without allocating
       a swap slot, and swap slot is full. */
    if (frame == NULL) {
      PANIC("No frame can be evicted without allocating a swap slot, and swap "
              "slot is full.\n");
    }
  } else {
    /* Otherwise, we can simply add the frame to the frame
       table (in an fte). */
    add_frame(frame, upage);
  }

  /* Return the kernel virtual address of the actual frame in the fte. */
  return frame;
}

/* Remove frame for this page from frame table, and then free the page
   itself. Called instead of palloc_free_page() (in process.c only??).
   Argument is return value of frame_alloc(). */
void
frame_free(void *frame) {
  remove_frame(frame);
  palloc_free_page(frame);
}

/* Called if frame table is full. Currently chooses a RANDOM frame in
   frame_table. */
struct fte *
choose_frame_to_evict(void) {
  int ftes = list_size(&frame_table); //Need to make sure we don't evict frames we have 'removed' if they stay in the list
  ASSERT(ftes > 0); //Should call when table is full (Don't know max size right now)
  int random_fte = (random_ulong()) % (ftes); //Random number between 0 and (size - 1) inclusive.
  struct list_elem *e;
  struct fte *fte;
  for (e = list_begin(&frame_table); random_fte > 0; random_fte--) {
    e = list_next(e);
    fte = list_entry(e, struct fte, fte_elem);
    //printf("frame is %p, upage is %p, owner is %d \n",fte->frame, fte->upage, fte->owner);
  }
  return fte;
}

/* Evict a frame. Returns a frame (the evicted frame), like frame_alloc() would have returned. Returns
   NULL on failure. */
void *
evict(void *upage) {
  struct fte *frame_entry = choose_frame_to_evict();
  save_frame(frame_entry, upage);
  ASSERT(frame_entry->frame != NULL);
  return frame_entry->frame;
}

static int m = 0;

void
save_frame(struct fte *frame, void *upage)
{
  printf("TID OWNER: %d\n", frame->owner);
  struct thread *t = tid_to_thread((tid_t) frame->owner);
  ASSERT(t != NULL);
  struct spt_entry *entry = get_spt_entry(&t->supp_pt, frame->upage);
  entry->vaddr = upage;
  frame->upage = upage;
  entry->frame_addr = frame->frame;
  ASSERT(entry != NULL);
  ASSERT(frame->upage == entry->vaddr);
  //printf("vaddr is %p \n", entry->vaddr);
  bool dirty = pagedir_is_dirty(&t->supp_pt, entry->vaddr);
  if (entry->file_info.executable && entry->info == FSYS) {
      entry->info = SWAP;
  }
  if (entry->info == SWAP || entry->info == ALL_ZERO) {
      m++;
      printf("Number of swap in to disk: %d\n", m);
      size_t swap_slot = swap_in(entry->frame_addr);
      entry->swap_slot = swap_slot;
      entry->frame_addr = frame->frame;
  } else if (entry->info == FSYS || entry->info == MMAP) {
      file_write_at(entry->file_info.f, entry->frame_addr, entry->file_info.size, entry->file_info.offset);
  }
  //ASSERT(frame->frame == entry->frame_addr);
  ASSERT(frame->upage == entry->vaddr);
  pagedir_clear_page(t->pagedir, frame->upage);
}


/* Creates a frame that will contain a pointer to the given page, and adds
   this frame to the frame table. Returns true if frame was successfully
   added, or false otherwise (i.e. if there was not enough memory to
   malloc space for a struct frame). Called in frame_alloc(). */
static bool
add_frame(void *frame, void *upage) {
  /* Frame is freed in remove_frame(). */
  struct fte *fte = malloc(sizeof(struct fte));
  if (fte == NULL) {
      PANIC("System can not allocate more frames.");
  }

  /* Return false if struct fte could not be successfully malloc'd. */
  if (fte == NULL) {
    return false;
  }

  /* Set members of struct fte. */
  fte->frame = frame;
  fte->upage = upage;
  fte->owner = (pid_t)thread_current()->tid;

  /* Add the created frame to the frame table. Must acquire a lock while
     accessing this list, because other threads could try to access this list
     at the same time. */
  lock_acquire(&frame_table_lock);
  list_push_back(&frame_table, &fte->fte_elem);
  lock_release(&frame_table_lock);

  /* Return true as the frame must have been successfully created to get to
     this line of code (as false would have been returned otherwise). */
  return true;
}

/* Removes the frame from the frame table that has the pointer to the
   supplied page in it. Called in frame_free(). */
static void
remove_frame(void *frame) {
  struct list_elem *e;
  struct fte *fte;

  /* Traverse frame table until we find a frame with a pointer to the
     supplied page. */
  lock_acquire(&frame_table_lock);
  for(e = list_begin(&frame_table);
      e != list_end(&frame_table);
      e = list_next(e)) {

    fte = list_entry(e, struct fte, fte_elem);

    if (fte->frame == frame) {
      list_remove(e);
      /* Ensure we free the fte, as we malloc'd space for this in
         add_frame(). */
      free(fte);
      break;
    }

  }
  lock_release(&frame_table_lock);
}

void
debug_frame(void) {
    int ftes = list_size(&frame_table);
    struct list_elem *e;
    int count = 0;
    printf("-----------------------------------\n\n");
    for (e = list_begin (&frame_table); e != list_end (&frame_table); e = list_next (e)) {
        count++;
        struct fte *fte = list_entry(e, struct fte, fte_elem);
        struct thread *t = tid_to_thread((tid_t) fte->owner);
        struct spt_entry *entry = get_spt_entry(&t->supp_pt, fte->upage);
        printf("%d: frame is: %p, vaddr is: %p, thread owner is: %d.\n", count, fte->frame, fte->upage, fte->owner);
    }
    printf("-----------------------------------\n\n");
}



