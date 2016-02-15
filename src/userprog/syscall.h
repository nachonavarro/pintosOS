#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

/* Maximum number of bytes allowed to be written to the console with a
   single putbuf() */
#define MAX_SINGLE_CONSOLE_WRITE 300

void syscall_init (void);

#endif /* userprog/syscall.h */
