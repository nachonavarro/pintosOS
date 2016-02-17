#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"


struct lock secure_file;

static void syscall_handler (struct intr_frame *);
static void sys_halt(void);
static void sys_exit(int status);
static void sys_exec(void);
static void sys_wait(void);
static void sys_create(void);
static void sys_remove(void);
static void sys_open(void);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(void);
static void sys_tell(void);
static void sys_close(void);

struct file* get_file(int fd);
static void check_mem_ptr(const void *uaddr);
static uint32_t get_word_on_stack(struct intr_frame *f, int offset);
static uint32_t write_word_to_stack(struct intr_frame *f, int offset,
                                                           uint32_t word);

struct proc_file {
  struct file *file;
  int fd;
  struct list_elem file_elem;
};

void
syscall_init (void) 
{
  lock_init(&secure_file);
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
			uint32_t status = get_word_on_stack(f, 1);
			sys_exit(status);
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

	// Not sure if parent thread will wake up.
	struct thread *cur = thread_current();
	if (cur->parent != NULL) {
		cur->parent->exit_status = status;
	}
	printf("%s: exit(%d) \n", cur->name, status);
	thread_exit();

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

static int
sys_filesize(int fd) {
	lock_acquire(&secure_file);
	struct file *f = get_file(fd);
	int length = file_length(f);
	lock_release(&secure_file);
	return length;
}

static int
sys_read(int fd, void *buffer, unsigned size) {
	lock_acquire(&secure_file);
	if (fd == 0) {
		// TODO: Implement input from console.
	}
	struct file *f = get_file(fd);
	int bytes = file_read(f, buffer, size);
	lock_release(&secure_file);
	return bytes;
}

/* Writes size bytes from buffer to the OPEN file fd. Returns the number
   of bytes actually written. Since file growth is not implemented, if we
   get to the end of the file, just stop writing and return the number of
   bytes already written. fd = 1 writes to the console. */
static int
sys_write(int fd, const void *buffer, unsigned size) {
  lock_acquire(&secure_file);
  if (fd == 1) {
    if (size < 300) {
      putbuf(buffer, size);
    } else {
      putbuf(buffer, 300);
      sys_write(fd, buffer, size - 300);
    }
    return size;
  }

  struct file *f = get_file(fd);
  check_valid_file(f);
  int bytes = file_write(f, buffer, size);
  lock_release(&secure_file);
  return bytes;

}

static void
sys_seek(int fd, unsigned position) {
	lock_acquire(&secure_file);
	struct file *f = get_file(fd);
	check_valid_file(f);
	file_seek(f, position);
	lock_release(&secure_file);
}

unsigned
sys_tell(int fd) {

}

static void
sys_close(void) {

}


struct file* get_file(int fd) {
	struct thread *cur = thread_current();
	struct list_elem *e;
	for (e = list_begin (&cur->files); e != list_end (&cur->files);
	     e = list_next (e)) {
		struct proc_file *f = list_entry(e, struct proc_file, file_elem);
		if (fd == f->fd) {
			return f->file;
		}
	}
}

/* Returns the word (4 bytes) at a given offset from a frames stack pointer.
   Only aligned word access is possible. */
static uint32_t get_word_on_stack(struct intr_frame *f, int offset) {

  check_mem_ptr(f->esp);
  check_mem_ptr(f->esp + offset);
  return *((uint32_t *)(f->esp) + offset); //TODO: Is uint32_t correct?
}

//static uint32_t write_word_to_stack(struct intr_frame *f, int offset,
//                                                           uint32_t word) {
//
//}

//TODO: Don't think we need to use pagedir_get_page, especially as we don't
//have a pd to pass
//TODO: Maybe this will be done by passing -1 exit status to parent?
static void
check_mem_ptr(const void *uaddr) {
  if (uaddr == NULL || !is_user_vaddr(uaddr)) {
    sys_exit(-1); //TODO: Is -1 correct? And do we exit with only arg as int
  }
}

void
check_valid_file(struct file *f) {
	if (!f) {
		lock_release(&secure_file);
		return;
	}
}


