#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <threads/synch.h>
#include "fixed-point.h"
#include "filesys/directory.h"
#include "vm/mmap.h"
#include "vm/page.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;

#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Number of priorities in Pintos. Used for the BSD Scheduler. */
#define NUM_PRIORITIES (PRI_MAX - PRI_MIN + 1)

/* BSD scheduler priority scaling factors */
/* priority calculation includes "- recent_cpu / RECENTCPU_DIVISOR"... */
#define RECENTCPU_DIVISOR 4
/* ...and "-nice*NICE_COEFFICIENT" */
#define NICE_COEFFICIENT 2
/* These are fixed_point coefficients for load_avg calculation. */
#define LOAD_AVG_COEFFICIENT DIV_FIXED_POINT_BY_INT(TO_FIXED_POINT(59), 60)
#define READY_THREAD_COUNT_COEFFICIENT       \
                              DIV_FIXED_POINT_BY_INT(TO_FIXED_POINT(1), 60)

#define FD_ERROR -1

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                         /* Thread identifier. */
    enum thread_status status;         /* Thread state. */
    char name[16];                     /* Name (for debugging purposes). */
    uint8_t *stack;                    /* Saved stack pointer. */
    int base_priority;                 /* Base priority. */
    int effective_priority;          /* Used in both default and mlfqs mode. */
    struct list_elem allelem;          /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;             /* List element. */

    struct semaphore timer_wait_sema;  /* Semaphore to make the thread wait. */

    struct list_elem sleep_elem;       /* list_elem for sleeping threads. */
    int64_t ticks_to_wake_on;

    struct list locks_holding;  /* List of locks that the thread is holding. */
    struct lock *waiting_on_lock;         /* Lock that thread is waiting on. */
    struct semaphore *waiting_on_sema;    /* Semaphore that thread
                                             is waiting on. */

    int nice;                          /* Thread niceness value */
    fixed_point recent_cpu;            /* CPU time recently received */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;              /* Page directory. */
    struct list children;           /* List of child threads of this thread. */
    struct list_elem child_elem;    /* list_elem for to be put in list of
                                       another thread's children. */
    bool waited_on;                 /* True if thread's parent has waited on
                                       this thread. Never set to false after
                                       being set to true, because we do not
                                       want to be able to wait on the same
                                       thread twice. */
    struct semaphore exit_sema;    /* Semaphore to ensure the wait system
                                      call will wait until the thread has
                                      exited. Initialised to 0. */
    struct semaphore before_exit_sema; /* Semaphore used to ensure that a
                                          thread cannot exit until we have
                                          obtained its exit status at the end
                                          of a call to process_wait().
                                          Intialised to 0 when a thread is
                                          created. sema_down called in
                                          thread_exit to stop the thread being
                                          terminated until sema_up is called at
                                          the end of process_wait. */
    struct semaphore load_sema;    /* Semaphore to ensure the exec system call
                                      does not check to see if the child has
                                      successfully loaded until it has tried
                                      to be loaded. */
    bool loaded;                   /* Set to the return value of load() in
                                      start_process(), so that sys_exec() can
                                      check whether the child loaded
                                      successfully or not, as if not, -1 should
                                      be returned from sys_exec(). loaded is
                                      set to false when the thread is
                                      created. */
    char executable[NAME_MAX]; /* In start_process(), if we load an executable
                                  on a thread, the thread's executable member
                                  will be set to the filename of this
                                  executable. */
#endif

    int exit_status;
    struct list files;            /* List of files that a thread has open (Same
                                     file can be open with different fd). */
    int next_file_descriptor;     /* Next file to be opened by this
                                     process/thread will take this as its'
                                     file descriptor. Incremented after a
                                     file is opened. */

#ifdef VM
    struct hash supp_pt; /* Hash map of virtual addresses to
                                additional information */
    /* Memory mapping members. */
    struct hash mmap_table; /* Mapping between mapid_t and struct mmap_mapping. */
    struct lock mmap_table_lock; /* Acquired/released before/after calling
                                    hash_insert()/hash_delete() on this
                                    threads mmap_table. */
    mapid_t next_mapid; /* Next mmap mapping for this thread will take this as its
                           mapid. Incremented after a new mapping is added.
                           Initially set to 0. */
#endif

    struct file *exec_file;

    /* Owned by thread.c. */
    unsigned magic;                    /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

fixed_point load_avg;                  /* System-wide load_avg variable. */

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

bool is_idle_thread(struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

struct thread *tid_to_thread(tid_t tid);

/* PART 1: Priority scheduling */
void thread_donate_priority (struct thread *t, int priority);
void thread_recalculate_effective_priority(struct thread *t);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);
void thread_set_priority (int);

/* Adds a thread to the appropriate ready-list */
/* in priority scheduling mode, there's only one,
   but in mlfqs there are more */
void add_to_ready_list(struct thread *t);

/* PART 2: BSD Scheduler. */

/* Get, calculate, and update priority. */
int thread_get_priority (void);
int thread_calculate_bsd_priority(fixed_point recent_cpu, int nice);
void thread_update_bsd_priority(struct thread *t, void *aux);

/* Get, calculate, and update recent CPU. */
int thread_get_recent_cpu (void);
fixed_point thread_calculate_recent_cpu(fixed_point cpu, int nice);
void thread_update_recent_cpu(struct thread *cur, void *aux);

/* Get, calculate, and update load average. */
int thread_get_load_avg (void);
fixed_point thread_calculate_load_avg(fixed_point load);
void thread_update_load_avg(void);

int thread_get_nice (void);
void thread_set_nice (int);

#endif /* threads/thread.h */
