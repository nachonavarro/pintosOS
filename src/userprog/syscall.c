#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"


struct lock secure_file;

static void syscall_handler (struct intr_frame *);
static void sys_halt(void);
static void sys_exit(int status);
static pid_t sys_exec(const char *cmd_line);
static void sys_wait(void);
static bool sys_create(const char *file, unsigned initial_size);
static bool sys_remove(const char *file);
static int sys_open(const char *file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);

/* Helper functions for system calls. */
struct file* get_file(int fd);
void check_valid_file(struct file *f);
static void check_mem_ptr(const void *uaddr);
static uint32_t get_word_on_stack(struct intr_frame *f, int offset);
//static uint32_t write_word_to_stack(struct intr_frame *f, int offset,
  //                                                         uint32_t word);

/* Process file. Each thread (i.e. process, as Pintos is not multithreaded)
 * has a list of proc_files to represent the file descriptors it has open. Two
 * different proc_files (even open in the same process) can have the same file
 * member, but a different fd, due to it being opened twice. */
struct proc_file {
  struct file *file;
  int* fd;
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
  /* Check to see that we can read supplied user memory pointer, using the
     check_mem_ptr helper() function, in get_word_from_stack(). If the
     check fails, the process is terminated. */
  //TODO: Do we need to check when writing? (Just when doing f->eax = ...??)

	int syscall_number = (int)get_word_on_stack(f, 0);

	switch(syscall_number) {
		case SYS_HALT:
			sys_halt();
			break;
		case SYS_EXIT:
			int status = (int)get_word_on_stack(f, 1);
			sys_exit(status);
			/* Returns exit status to the kernel. */
			f->eax = status;
			break;
		case SYS_EXEC:
			pid_t pid = sys_exec("const char *cmd_line");
			/* Returns new processes pid. */
			f->eax = pid;
			break;
		case SYS_WAIT:
			sys_wait();
			break;
		case SYS_CREATE:
			const char *filename  = (const char *)get_word_on_stack(f, 1);
			unsigned initial_size = (unsigned)get_word_on_stack(f, 2);
			/* Returns true to the kernel if creation is successful. */
			f->eax = (int)sys_create(filename, initial_size);
			break;
		case SYS_REMOVE:
			const char *filename = (const char *)get_word_on_stack(f, 1);
			f->eax = sys_remove(filename);
			break;
		case SYS_OPEN:
			const char *filename = (const char *)get_word_on_stack(f, 1);
			f->eax = sys_open(filename);
			break;
		case SYS_FILESIZE:
			int fd = (int)get_word_on_stack(f, 1);
			f->eax = sys_filesize(fd);
			break;
		case SYS_READ:
			int fd        = (int)get_word_on_stack(f, 1);
			void *buffer  = (void *)get_word_on_stack(f, 2);
			unsigned size = (unsigned)get_word_on_stack(f, 3);
			f->eax = sys_read(fd, buffer, size);
			break;
		case SYS_WRITE:
			int fd        = (int)get_word_on_stack(f, 1);
			void *buffer  = (void *)get_word_on_stack(f, 2);
			unsigned size = (unsigned)get_word_on_stack(f, 3);
			f->eax = sys_write(fd, buffer, size);
			break;
		case SYS_SEEK:
			int fd             = (int)get_word_on_stack(f, 1);
			unsigned position  = (int)get_word_on_stack(f, 2);
			sys_seek(fd, position);
			break;
		case SYS_TELL:
			int fd        = (int)get_word_on_stack(f, 1);
			f->eax = sys_tell(fd);
			break;
		case SYS_CLOSE:
			sys_close();
			break;
		default: NOT_REACHED();
	}

}

/* Terminates Pintos by calling shutdown_power_off().
   Should rarely be used, as some information is lost
   (such as information about possible deadlocks). */
static void
sys_halt(void) {
	shutdown_power_off();
}

/* Terminates the current user program. If the processes parent waits for it,
   this status will be returned to the parent (This is what the parent is
   waiting for). Status of 0 indicates success, and anything else indicates
   an error. After this function is called in syscall_handler(), the exit
   status is sent ('returned') to the kernel. */
static void
sys_exit(int status) {

	struct thread *cur = thread_current();
	//TODO: Not sure if parent thread will wake up, or if this is the correct
	//      way to return the status to the parent.
	if (cur->parent != NULL) {
		cur->parent->exit_status = status;
	}
	/* Process termination message, printing process' name and exit status. */
	printf("%s: exit(%d) \n", cur->name, status);
	thread_exit();

}

//TODO: Pretty sure we already cannot return from exec until we know whether
//      child has loaded successfully or not, although spec says we need to use
//      synchronisation to ensure this (which is why the lock stuff is
//      commented out).
//TODO: Is the new process set as the child of the current thread already (in
//      process_execute())? Or do we need to do it here?
/* Runs the executable given in cmd_line. cmd_line includes the arguments as
   well. Returns the new process' pid. Returns -1 if the process could not
   load or run for some reason, which is returned from process_execute().
   After this function is called in syscall_handler(), the new process' id
   is sent to the kernel. */
static pid_t
sys_exec(const char *cmd_line) {
//  lock_acquire(&secure_file);
  /* Identity mapping between thread id and process id, because
     Pintos is not multithreaded. */
  pid_t pid = (pid_t)process_execute(cmd_line);
//  lock_release(&secure_file);

  return pid;
}

static void
sys_wait(void) {

}

/* Creates file called 'file' that is initially 'initial_size_ bytes.
   Returns true if successful. Creating a file does NOT open it. */
static bool
sys_create(const char *file, unsigned initial_size) {
	lock_acquire(&secure_file);
	bool success = filesys_create(file, initial_size);
	lock_release(&secure_file);
	return success;
}

/* Removes file called 'file'. Returns true if successful, and false otherwise
   (including when no file named 'file' exists). An open file can be removed,
   but it is not closed. */
static bool
sys_remove(const char *file) {
	lock_acquire(&secure_file);
	bool success = filesys_remove(file);
	lock_release(&secure_file);
	return success;

}

/* Opens specified file. Returns its file descriptor. Same file opened
   multiple times returns different file descriptors. A process has an
   independent set of file descriptors (files open in that process). fd = 0 is
   STDIN_FILENO, fd = 1 is STDOUT_FILENO - These are never returned here.
   If file could not be opened, -1 is returned. */
static int
sys_open(const char *file) {
	lock_acquire(&secure_file);
	struct file *fl = filesys_open(file);
	if (fl == NULL) {
	  return -1;
	}
	check_valid_file(fl);
	struct proc_file *f = malloc(sizeof(struct proc_file)); // TODO: REMEMBER WE NEED TO FREE SOMEWHERE.
	list_push_front(&thread_current()->files, &f->file_elem);
	f->file = fl;
	int file_descriptor = -1 ; //TODO: How do we assign a fd?
	f->fd = file_descriptor;
	lock_release(&secure_file);

	return file_descriptor;
}

/* Returns size, in bytes, of file open as 'fd'. */
static int
sys_filesize(int fd) {
	lock_acquire(&secure_file);
	struct file *f = get_file(fd);
	int length = file_length(f);
	lock_release(&secure_file);
	return length;
}

/* Reads 'size' bytes from file open as fd into buffer. Returns number of
   bytes actually read - 0 at end of file, or -1 if file could not be read.
   fd = 0 reads from the keyboard. */
static int
sys_read(int fd, void *buffer, unsigned size) {
  //TODO: Do we need to check fd is valid?
  int bytes;
	lock_acquire(&secure_file);
	if (fd == 0) {
	  int i;
	  uint8_t keys[size];
	  for (i = 0; i < size; i++) {
	    keys[size] = input_getc();
	  }
	  memcpy(buffer, (const void *) keys, size);
	  bytes = size;
	} else {
	  struct file *f = get_file(fd);
	  bytes = file_read(f, buffer, size);
	}
	lock_release(&secure_file);
	return bytes;
}

/* Writes size bytes from buffer to the OPEN file fd. Returns the number
   of bytes actually written. Since file growth is not implemented, if we
   get to the end of the file, just stop writing and return the number of
   bytes already written. fd = 1 writes to the console. */
static int
sys_write(int fd, const void *buffer, unsigned size) {
  int bytes;
  lock_acquire(&secure_file);
  if (fd == 1) {
    if (size < 300) {
      putbuf(buffer, size);
    } else {
      putbuf(buffer, 300);
      sys_write(fd, buffer, size - 300);
    }
    bytes = size;
  } else {
    struct file *f = get_file(fd);
    bytes = file_write(f, buffer, size);
  }

  struct file *f = get_file(fd);
  check_valid_file(f);
  int bytes = file_write(f, buffer, size);
  lock_release(&secure_file);
  return bytes;

}

/* Changes the next byte to be read or written in open file 'fd' to
   'position' (in bytes, from start of file). */
static void
sys_seek(int fd, unsigned position) {
	lock_acquire(&secure_file);
	struct file *f = get_file(fd);
	check_valid_file(f);
	file_seek(f, position);
	lock_release(&secure_file);
}

/* Returns the position of the next byte to be read or written in open
   file 'fd' (in bytes, from start of file). */
unsigned
sys_tell(int fd) {
	lock_acquire(&secure_file);
	struct file *f = get_file(fd);
	check_valid_file(f);
	int position = file_tell(f);
	lock_release(&secure_file);
	return position;
}

//TODO: Do we need to actually close all open file descriptors?
//      Or is it already done? Spec is a bit vague...
/* Closes file descriptor fd. */
static void
sys_close(int fd) {
	lock_acquire(&secure_file);
	struct thread *cur = thread_current();
	struct list_elem *e;
	for (e = list_begin (&cur->files); e != list_end (&cur->files);
	     e = list_next (e)) {
		struct proc_file *f = list_entry(e, struct proc_file, file_elem);
		if (fd == f->fd) {
			file_close(f->file);
			list_remove(f->file_elem);
		}
	}
	lock_release(&secure_file);
}

/* Returns the file corresponding the supplied file descriptor
   'in the current thread's list of files that it can see. */
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
	NOT_REACHED();
	return NULL;
}

/* Returns the word (4 bytes) at a given offset from a frames stack pointer.
   Only aligned word access is possible because the stack pointer is cast
   from a (void *) to a (uint32_t *). */
static uint32_t get_word_on_stack(struct intr_frame *f, int offset) {
  check_mem_ptr(f->esp);
  check_mem_ptr(f->esp + offset);
  return *((uint32_t *)(f->esp) + offset);
}

/* If supplied pointer is a null pointer, not in the user address space, or
   is an unmapped virtual address, the process is terminated. */
static void
check_mem_ptr(const void *uaddr) {
  if (uaddr == NULL || !is_user_vaddr(uaddr)
      || pagedir_get_page(thread_current()->pagedir, uaddr) != NULL) {
    sys_exit(-1);
  }
}

//TODO: What does this do? It won't stop the function that called this from
//      running, it will just release the lock, so it can be interrupted.
/* PRE: We have acquired the lock to file system. */
void
check_valid_file(struct file *f) {
	if (!f) {
		lock_release(&secure_file);
		return;
	}
}


