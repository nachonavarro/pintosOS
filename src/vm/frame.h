#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "userprog/process.h"
#include "threads/thread.h"

struct fte {
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
void *alloc_frame(void *upage);
void free_frame(void *frame);
struct fte *choose_frame_to_evict(void);
void *evict(void *upage);

#endif /* vm/frame.h */

