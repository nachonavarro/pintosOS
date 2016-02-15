#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);
static void sys_halt(void);
static void sys_exit(int status);
static void sys_exec(void);
static void sys_wait(void);
static void sys_create(void);
static void sys_remove(void);
static void sys_open(void);
static void sys_filesize(void);
static void sys_read(void);
static void sys_write(void);
static void sys_seek(void);
static void sys_tell(void);
static void sys_close(void);

static void check_mem_ptr(const void *uaddr);
static uint32_t get_word_on_stack(struct intr_frame *f, int offset);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  //TODO: Must check that we can READ supplied user memory ptr here
  //Only check if we can WRITE if the system call requires writing
  //If check fails, must terminate process (exit??) and free its resources
  //check_mem_ptr((const void *)f->esp);
  //^^THIS IS NOW CHECK IN get_word_on_stack()

  //TODO: Cast to an int* first? Not sure if you can dereference a void*
	int syscall_number = get_word_on_stack(f, 0);

	switch(syscall_number) {
		case SYS_HALT:
			sys_halt();
			break;
		case SYS_EXIT:
			sys_exit(-1); //-1 is placeholder
			break;
		case SYS_EXEC:
			sys_exec();
			break;
		case SYS_WAIT:
			sys_wait();
			break;
		case SYS_CREATE:
			sys_create();
			break;
		case SYS_REMOVE:
			sys_remove();
			break;
		case SYS_OPEN:
			sys_open();
			break;
		case SYS_FILESIZE:
			sys_filesize();
			break;
		case SYS_READ:
			sys_read();
			break;
		case SYS_WRITE:
			sys_write();
			break;
		case SYS_SEEK:
			sys_seek();
			break;
		case SYS_TELL:
			sys_tell();
			break;
		case SYS_CLOSE:
			sys_close();
			break;
		default: NOT_REACHED();
	}

}


//SETTING ALL FUNCTIONS TO RETURN VOID FOR NOW. IT WILL DEPEND ON THE FUNCTION, THOUGH.
static void
sys_halt(void) {
	shutdown_power_off();
}

static void
sys_exit(int status) {

}

static void
sys_exec(void) {

}

static void
sys_wait(void) {

}

static void
sys_create(void) {

}

static void
sys_remove(void) {

}

static void
sys_open(void) {

}

static void
sys_filesize(void) {

}

static void
sys_read(void) {

}

static void
sys_write(void) {

}

static void
sys_seek(void) {

}

static void
sys_tell(void) {

}

static void
sys_close(void) {

}

/* Returns the word (4 bytes) at a given offset from a frames stack pointer.
   Ensures that only aligned word access is possible. */
static uint32_t get_word_on_stack(struct intr_frame *f, int offset) {
  ASSERT((offset % 4) == 0);

  check_mem_ptr(f->esp);
  check_mem_ptr(f->esp + offset);
  return *((uint32_t *)(f->esp + offset)); //TODO: Is uint32_t correct?
}

//TODO: Don't think we need to use pagedir_get_page, especially as we don't
//have a pd to pass
//TODO: Maybe this will be done by passing -1 exit status to parent?
static void
check_mem_ptr(const void *uaddr) {
  if (uaddr == NULL || !is_user_vaddr(uaddr)) {
    sys_exit(-1); //TODO: Is -1 correct? And do we exit with only arg as int
  }
}
