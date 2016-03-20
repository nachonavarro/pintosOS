#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/palloc.h"

struct fte {
  /* frame and upage are such that install_page(upage, kpage, _) will be
     called after frame_alloc() is called (kpage is returned from frame_alloc()
     and upage is passed as an argument, but both are stored in the fte). */
  void *frame; /* The frame itself, as the frame is 'just a page'. */
  void *upage; /* Pointer to page that currently occupies this frame. */

  //Page table will have mapping from upage (user virtual address) to frame
  //(kpage - kernel virtual address - how we access frame as frame is region
  //of contiguous physical memory and there is a mapping from physical memory
  //to kernel virtual memory [identity??])

  pid_t owner; /* pid of process that owns this frame. */
  struct list_elem fte_elem; /* To allow each frame to be added to 'static
                                  struct list frames' in 'frame.c'. */
};

void frame_table_init(void);
void *frame_alloc(enum palloc_flags flags, void *upage);
void frame_free(void *frame);
struct fte *choose_frame_to_evict(void);
void save_frame(struct fte *, void*);
void *evict(void *upage);

#endif /* vm/frame.h */

