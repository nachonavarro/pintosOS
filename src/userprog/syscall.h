#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "filesys/file.h"
#include "lib/kernel/hash.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

/* Map region identifier. */
typedef int mapid_t;

/* Maximum number of bytes allowed to be written to the console with a
   single putbuf() */
#define MAX_CONSOLE_WRITE 300
#define ERROR -1

/* Process file. Each thread (i.e. process, as Pintos is not multithreaded)
   has a list of proc_files to represent the file descriptors it has open. Two
   different proc_files (even open in the same process) can have the same file
   member, but a different fd, due to it being opened twice. */
struct proc_file {
  struct file *file;
  int fd;
  struct list_elem file_elem;
};

void syscall_init (void);
void sys_exit (int status);
void munmap_exiting(struct hash_elem *, void *);

#endif /* userprog/syscall.h */
