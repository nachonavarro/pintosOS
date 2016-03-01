#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* Struct holding all information needed to start a process. */
struct process_info {
  char *filename;
  int   argc;
  char *argv[];
};

#endif /* userprog/process.h */
