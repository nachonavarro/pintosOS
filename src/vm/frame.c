#include <stdbool.h>
#include "vm/frame.h"
#include "lib/random.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

static struct list frame_table;
static struct lock frame_table_lock;

static bool add_frame(void *frame, void *upage);
static void remove_frame(void *frame);

/* Initialise the actual frame table itself, along with any locks required in
   accessing the frame table. */
void
frame_table_init(void) {
  list_init(&frame_table);
  lock_init(&frame_table_lock);
}

//TODO: Change calls to palloc_get_page to allocate_frame (in process.c only??)
//      where it will be set to a new member in the struct fte?
//      I think we do, but not sure why we need this upage...
//      (This upage may just be the pointer to the user page, if any, that
//      occupies it)
/* Called instead of palloc_get_page(). Gets a page and then adds a frame to
   the frame table that points to that page. Calls a function to handle
   eviction if the frame table is full. Returns the page returned from
   palloc_get_page(), or the return value of evict(). Panics if no frame
   can evicted without allocating a swap slot, and swap slot is full. */
void *
allocate_frame(void *upage) {
  /* A frame is just a page sized region of physical memory, accessed through
     kernel virtual memory (returned from palloc_get_page()) */
  void *frame = palloc_get_page(PAL_USER);

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
    //TODO: What do we do if add_frame() can't malloc enough space and
    //      returns false? Panic?
  }

  /* Return the kernel virtual address of the actual frame in the fte. */
  return frame;
}

/* Remove frame for this page from frame table, and then free the page
   itself. Called instead of palloc_free_page() (in process.c only??).
   Argument is return value of allocate_frame(). */
void
free_frame(void *frame) {
  remove_frame(frame);
  palloc_free_page(frame);
}

/* Called if frame table is full. Currently chooses a RANDOM frame in
   frame_table. */
struct fte *
choose_frame_to_evict(void) {
  int ftes = list_size(&frame_table); //Need to make sure we don't evict frames we have 'removed' if they stay in the list
  ASSERT(ftes > 0); //Should call when table is full (Don't know max size right now)
  int random_fte = ((int) random_ulong()) % (ftes); //Random number between 0 and (size - 1) inclusive.
  struct list_elem *e;
  for (e = list_begin(&frame_table); random_fte > 0; random_fte--) {
    e = list_next(e);
  }
  struct fte *fte = list_entry(e, struct fte, fte_elem);
  return fte;
}

/* Evict a frame. Returns a frame (the evicted frame), like allocate_frame() would have returned. Returns
   NULL on failure. */
void *
evict(void *upage UNUSED) {
  //TODO: Needs implemeting - Calls choose_frame_to_evict() first?
  return NULL;
}

/* Creates a frame that will contain a pointer to the given page, and adds
   this frame to the frame table. Returns true if frame was successfully
   added, or false otherwise (i.e. if there was not enough memory to
   malloc space for a struct frame). Called in allocate_frame(). */
static bool
add_frame(void *frame, void *upage) {
  /* Frame is freed in remove_frame(). */
  struct fte *fte = malloc(sizeof(struct fte));

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
   supplied page in it. Called in free_frame(). */
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
