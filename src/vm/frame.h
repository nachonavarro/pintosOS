#ifndef VM_FRAME_H
#define VM_FRAME_H

struct frame {
  void *page; /* Pointer to page that currently occupies this frame. */
  pid_t owner; /* pid of process that owns this frame. */

  struct list_elem frame_elem; /* To allow each frame to be added to 'static
                                  struct list frames' in 'frame.c'. */
};

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

