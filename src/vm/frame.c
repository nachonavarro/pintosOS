#include "vm/frame.h"
#include "lib/random.h"

static struct list frame_table;

void frame_table_init(void) {
  list_init(&frame_table);
  //lock_init...
}

bool add_frame(void *page) {
  /* Frames used for user pages should be obtained from the user pool. */
  struct frame f = malloc(sizeof(struct frame)); //Should this be malloc(sizeof(struct frame))? And page should be made using palloc??
  if (f == NULL) {
    return false;
  }
  f->page = page;
  f->owner = (pid_t)thread_current()->tid;
  //NEED LOCK HERE, AS LIST IS 'GLOBAL'
  list_push_back(&frame_table, &f->frame_elem);
  return true;
}

void remove_frame(void *page) {
  struct list_elem *e;
  struct frame *f;
  //lock_acquire
  for(e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
    f = list_entry(e, struct frame, frame_elem);
    if (f->page == page) {
      list_remove(e);
      free(f); //Change to free if we malloc frames instead of use get_page
      //Do we need to free the page as well??
      break;
    }
  }
  //lock_release
}

//Called instead of palloc_get_page(). Adds the page to an entry in the frame
//table. Eviction will happen if frame table is full. Returns the same thing
//that palloc_get_page() would return, although takes eviction into account.
//TODO: Change calls to palloc_get_page to alloc_frame (in process.c only??)
void *alloc_frame(void) {
  void *page = palloc_get_page(PAL_USER);
  if (page == NULL) {
    //TODO: Frame table is full(?) -> Evict!
    //PANIC if no frame can  be evicted without allocating a swap slot, and swap slot is full
    //If eviction successful, must set page to the correct thing now, so we can return it
  } else {
    add_frame(page); //Otherwise, add to the frame table
  }
  return page;
}

/* Called if frame table is full. Currently chooses a RANDOM frame in
   frame_table. */
struct frame *choose_frame_to_evict(void) {
  int frames = list_size(&frame_table); //Need to make sure we don't evict frames we have 'removed' if they stay in the list
  ASSERT(frames > 0); //Should call when table is full (Don't know max size right now)
  int random_frame = ((int) random_ulong()) % (frames); //Random number between 0 and (size - 1) inclusive.
  struct list_elem *e;
  for (e = list_begin(&frame_table); random_frame > 0; random_frame--) {
    e = list_next(e);
  }
  struct frame *frame = list_entry(e, struct frame, frame_elem);
  return frame;
}

