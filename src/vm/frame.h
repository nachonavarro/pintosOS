#ifndef VM_FRAME_H
#define VM_FRAME_H

//TODO: Do we need a paddr and vaddr OR upage and vpage? Is this just frame
//      and upage?...
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

//TODO: Put functions in header - not static ones.
/*
  bool add_frame(void *frame, void *uaddr); //Add frame to table
  void remove_frame(void *frame); //Remove frame from table (and call
                                  //palloc_free_page()?? OR put this in
                                  //separate free_frame function?? Or just
                                  //call palloc_free_page where we need it??)
  struct frame get_frame(void *frame); //Retrieve frame table entry that points to given page
  void *frame_alloc(enum palloc_flags flags, void *uaddr); //Attempt to allocate a frame
                                                           //Returns frame allocated using
                                                           //palloc_get_page().
  void *evict(void *uaddr); //Evict a frame.
 */

#endif /* vm/frame.h */

