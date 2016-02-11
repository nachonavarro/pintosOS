#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  printf ("system call!\n");
  thread_exit ();
  f->esp
}

static void
read_user_memory(uint32_t *pd, const void *uaddr) {
  check_mem_ptr(pd, uaddr);

}

static void
write_user_memory(uint32_t *pd, const void *uaddr) {
  check_mem_ptr(pd, uaddr);

}

static void
check_mem_ptr(uint32_t *pd, const void *uaddr) {
  ASSERT(uaddr != NULL);
  ASSERT(is_user_vaddr(uaddr));
  ASSERT(pagedir_get_page(pd, uaddr) != NULL);
}
