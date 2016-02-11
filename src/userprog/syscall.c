#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void sys_halt(void);
static void sys_exit(void);
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
  check_mem_ptr(f->esp);

	int syscall_number = *(f->esp);

	switch(syscall_number) {
		case SYS_HALT:
			sys_halt();
			break;
		case SYS_EXIT:
			sys_exit();
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
sys_exit(void) {

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

static void
check_mem_ptr(uint32_t *pd, const void *uaddr) {
  ASSERT(uaddr != NULL);
  ASSERT(is_user_vaddr(uaddr));
  ASSERT(pagedir_get_page(pd, uaddr) != NULL);
}
