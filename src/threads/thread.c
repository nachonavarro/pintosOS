#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running.
   In mlfqs mode, ready_lists_bsd does this, otherwise ready_list
   will do this this. */

/* Array of ready_lists depending on priority for BSD Scheduler */
static struct list ready_lists_bsd[NUM_PRIORITIES];
static struct list ready_list;

/* Simply the number of threads in ready_lists_bsd.
   Needed to calculate load_avg. Only used in mlfqs mode. */
static int num_of_ready_threads;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

static int highest_ready_priority(void);


/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  /* Initialise the 64 queues */
  if (thread_mlfqs) {
    int i;

    for (i = 0; i < (NUM_PRIORITIES); ++i) {
      list_init(&ready_lists_bsd[i]);
    }

    /* load_avg set to 0 on OS boot. */
    load_avg = 0;
    /* Number of ready threads is set to 0 on boot as well. */
    num_of_ready_threads = 0;
    
  } else {
    list_init (&ready_list);
  }

  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Next if statement deals with updating BSD Scheduler specific data, such as
     recent_cpu. */
  if (thread_mlfqs) {
    struct thread *cur = thread_current();

    /* Increment recent_cpu by 1 except if the idle thread is running */
    if (!is_idle_thread(cur)) {
      thread_current()->recent_cpu
        = ADD_INT_AND_FIXED_POINT(1, thread_current()->recent_cpu);
    }
    
    /* Every second update load_avg and recent_cpu of all threads.
       Only need to update priority if recent_cpu has changed, which
       happens every second, or every tick for the current thread.
       Note, we don't need to worry about the niceness having changed,
       which would mean we have to recalculate priority, because
       thread_set_nice() will automatically recalculate the priority. */
    if (timer_ticks() % TIMER_FREQ == 0) {
      thread_update_load_avg();
      thread_foreach(thread_update_recent_cpu, NULL);
      thread_foreach(thread_update_bsd_priority, NULL);
    } else if (timer_ticks() % TIME_SLICE == 0) {
      thread_update_bsd_priority(thread_current(), NULL);
    }
  }

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE) {
    intr_yield_on_return ();
  }
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);

  if (t == NULL) {
    return TID_ERROR;
  }

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack'
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  /* Yield if new thread's priority is higher */
  if (thread_get_priority() < t->effective_priority) {
    thread_yield();
  }

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  if (thread_mlfqs && !is_idle_thread(thread_current())) {
    num_of_ready_threads--;
  }

  thread_current()->status = THREAD_BLOCKED;
  schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);

  if (thread_mlfqs) {
    num_of_ready_threads++;
  }

  add_to_ready_list(t);

  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();

  if (thread_mlfqs) {
    num_of_ready_threads--;
  }

  list_remove (&thread_current()->allelem);
  struct thread *cur = thread_current();
  cur->status = THREAD_DYING;
  sema_up(&cur->exit_sema);
  schedule ();
  NOT_REACHED ();
}

/* Add t to the relevant ready_list - if using priority scheduling,
   that's easy but it's a little more complex with 4.4BSD scheduling. */
void add_to_ready_list(struct thread *t) 
{
  if (thread_mlfqs) {
    list_push_front(&ready_lists_bsd[t->effective_priority-PRI_MIN], &t->elem);
  } else {
    list_insert_ordered(&ready_list, &t->elem, less_priority, NULL);
  }
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();

  if (cur != idle_thread) {
    add_to_ready_list(cur);
  }

  cur->status = THREAD_READY;
  schedule();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); 
       e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority)
{
  /* Ignore if using BSD scheduler */
  if (thread_mlfqs) {
    return;
  }

  struct thread *t = thread_current();

 /* Always sets the base priority to new_priority. */
  t->base_priority = new_priority;

  /* If base_priority is now larger than the threads effective priority,
     its effective priority can now be changed to be equal to the base
     priority, as effective priority is the maximum of base priority
     and donated priorities. */
  if (t->base_priority > t->effective_priority) {
    t->effective_priority = t->base_priority;
  }

  /* Recalculate effective priority of thread based on the locks it is
     holding. */
  thread_recalculate_effective_priority(t);

  /* Check if we need to yield to let the new thread immediately
     start running. */
  if (!list_empty(&ready_list)) {
    struct thread *next_to_run 
      = list_entry(list_rbegin(&ready_list), struct thread, elem);
    if (next_to_run->effective_priority > new_priority) {
      if (intr_context()) {
        intr_yield_on_return();
      } else {
        thread_yield();
      }
    }
  }
}

void
thread_donate_priority (struct thread *t, int priority) 
{
  if (t->effective_priority >= priority) {
    return;
  }

  t->effective_priority = priority;

  /* If we change the priority of an element in an ordered list, we
     need to remove that element and then reinsert it in the new correct
     position in the list, so that the list is still ordered. */

  if (t->status == THREAD_BLOCKED) {
    ASSERT(!list_empty(&t->waiting_on_sema->waiters));

    list_remove(&t->elem);
    list_insert_ordered(&t->waiting_on_sema->waiters, &t->elem,
        less_priority, NULL);
  } else if (t->status == THREAD_READY) {
    ASSERT(!list_empty(&ready_list));

    list_remove(&t->elem);
    add_to_ready_list(t);
  }

  /* If the thread we are donating to is also waiting on a lock, we can
     donate the priority to the other locks holder as well. */
  if (t->waiting_on_lock != NULL) {
    ASSERT(&t->waiting_on_lock->holder != NULL);
    thread_donate_priority(t->waiting_on_lock->holder, priority);
  }
}

/* Sets threads effective priority to the largest priority
   out of the priorities of all locks that the thread is
   holding. Priority of a lock is the largest out of the
   priorities of its waiters. */
void
thread_recalculate_effective_priority(struct thread *t) 
{
  int max = t->base_priority;
  struct list_elem *e = NULL;

  if (list_empty(&t->locks_holding)) {
    t->effective_priority = t->base_priority;
    return;
  }

  /* Traverse through list of locks, locks_holding, setting max to
     the largest effective priority out of the effective priorities
     of all lock's waiters. */
  for (e = list_begin(&t->locks_holding); 
       e != list_end(&t->locks_holding);
       e = list_next(e)) {

    struct lock *lock = list_entry(e, struct lock, lock_elem);

    if (!list_empty(&lock->semaphore.waiters)) {
      int local_max = list_entry(list_rbegin(&lock->semaphore.waiters),
                                 struct thread, elem)->effective_priority;

      if (local_max > max) {
        max = local_max;
      }
    }
  }

  t->effective_priority = max;

  if (thread_get_priority() < highest_ready_priority()) {
    thread_yield();
  }
}

/* BSD SCHEDULER FUNCTIONS: */

/* Sets the current thread's nice value to NEW_NICE. */
void
thread_set_nice (int nice)
{
  ASSERT (nice <= 20);
  ASSERT (nice >= -20);

  /* Disable interrupts when setting nice */
  enum intr_level old_level = intr_disable();

  struct thread *t = thread_current();
  t->nice = nice;
  /* Must update a threads priority when setting its nice value. */
  thread_update_bsd_priority(t, NULL);

  intr_set_level(old_level);

  /* Yield if not highest priority */
  if (thread_get_priority() < highest_ready_priority()) {
    if (!intr_context()){
      thread_yield();
    } else {
      intr_yield_on_return();
    }
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current()->nice;
}

/* Returns the current thread's priority. */
int
thread_get_priority (void)
{
  return thread_current()->effective_priority;
}


/* Applies the formula: priority = PRI_MAX - (recent_cpu / 4) - (nice * 2). */
int
thread_calculate_bsd_priority(fixed_point recent_cpu, int nice)
{
  int effective_priority;
  fixed_point cpu_term 
      = DIV_FIXED_POINT_BY_INT(recent_cpu, RECENTCPU_DIVISOR);
  int nice_term = nice * NICE_COEFFICIENT;
  effective_priority = PRI_MAX - TO_INT_ROUND_ZERO(cpu_term) - nice_term;
    /* If priority calculated is invalid, it is rounded to the nearest valid
       priority. */

  if (effective_priority < PRI_MIN) {
    effective_priority = PRI_MIN;
  } else if (effective_priority > PRI_MAX) {
    effective_priority = PRI_MAX;
  }

  return effective_priority;
}

/* Recalculates a thread's priority using its nice and recent_cpu parameters */
void
thread_update_bsd_priority (struct thread *t, void *aux UNUSED)
{
  ASSERT (thread_mlfqs);

  t->effective_priority = thread_calculate_bsd_priority(t->recent_cpu, t->nice);

  /* If a threads priority changes in mlfqs mode, we need to move it
     to a different priority queue in ready_lists_bsd. */
  if (!is_idle_thread(t) && t->status == THREAD_READY) {
    list_remove(&t->elem);
    add_to_ready_list(t);
  }
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void)
{
  fixed_point rcpu = MUL_INT_AND_FIXED_POINT(100, thread_current()->recent_cpu);
  return TO_INT_ROUND_TO_NEAREST(rcpu);
}

/* Applies the formula: 
  recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice  
*/

fixed_point
thread_calculate_recent_cpu(fixed_point recent_cpu, int thread_nice) 
{
  fixed_point lavg_part_numerator
      = MUL_INT_AND_FIXED_POINT(2, load_avg);

  fixed_point lavg_part_denominator
      = ADD_INT_AND_FIXED_POINT(1, lavg_part_numerator);

  fixed_point lavg_part_fraction
      = DIV_FIXED_POINTS(lavg_part_numerator, lavg_part_denominator);

  fixed_point lavg_cpu_mult
      = MUL_FIXED_POINTS(lavg_part_fraction, recent_cpu);

  fixed_point rcpu
      = ADD_INT_AND_FIXED_POINT(thread_nice, lavg_cpu_mult);

  return rcpu;
}

/* Updates current thread's recent_cpu value. */
void
thread_update_recent_cpu(struct thread *cur, void *aux UNUSED) 
{
  if (!is_idle_thread(cur)) {
    cur->recent_cpu = thread_calculate_recent_cpu(cur->recent_cpu, cur->nice);
  }
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void)
{
  fixed_point lavg = MUL_INT_AND_FIXED_POINT(100, load_avg);
  return TO_INT_ROUND_TO_NEAREST(lavg);
}

/* Applies the formula: load_avg = (59/60)*load_avg + (1/60)*ready_threads */
fixed_point
thread_calculate_load_avg(fixed_point load)
{
  ASSERT(intr_get_level() == INTR_OFF);

  fixed_point load_part  = DIV_FIXED_POINT_BY_INT(TO_FIXED_POINT(59), 60);
  fixed_point ready_part = DIV_FIXED_POINT_BY_INT(TO_FIXED_POINT(1), 60);
  load  = MUL_FIXED_POINTS(load_part, load) 
        + MUL_INT_AND_FIXED_POINT(num_of_ready_threads, ready_part);

  return load;
}

void
thread_update_load_avg(void)
{
  load_avg = thread_calculate_load_avg(load_avg);
}

/* Returns highest priority out of all threads that are ready. */
static int 
highest_ready_priority(void)
{
  /* In the advanced scheduler, we must find the highest priority non-empty
     queue in the array of queues (ready_lists_bsd), and return this priority.
     Otherwise, we can return the effective priority of the first thread in the
     ordered ready list. */
  if (thread_mlfqs) {

    int i;

    for (i = NUM_PRIORITIES - 1; i >= 0; i--) {
      if (!list_empty(&ready_lists_bsd[i])) {
        return i + PRI_MIN;
      }
    }
    /* If no threads are ready, return -1. However, this function
       should not really be called in the case of no threads being ready. */
    return -1;

  } else {
    return (list_entry(list_rbegin(&ready_list),
                       struct thread, elem)->effective_priority);
  }

  NOT_REACHED();
  return -1;
}



/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

#ifdef USERPROG
  list_init(&t->children);
  t->waited_on = false;
  //TODO: Should we move this to process_execute?
  //      (And would this stop us having to use strcmp for
  //      main below? Would it still work in all other cases?)
  if (!strcmp(name, "main")) {
    list_push_front(&thread_current()->children, &t->child_elem);
  }
#endif

  if(!thread_mlfqs) {
    t->base_priority = t->effective_priority = priority;
  }
    
  list_init(&t->locks_holding);

  /* Initialise semaphore to 0 to synchronise sleeping threads. */
  t->waiting_on_lock = NULL;
  sema_init(&t->timer_wait_sema, 0);
  /* sema_down() called on exit_sema in process_wait(). sema_up() only
     called in thread_exit(). This means the wait system call cannot
     return the exit status until thread has terminated. */
  sema_init(&t->exit_sema, 0);
  t->magic = THREAD_MAGIC;

  /* First file descriptor for a process' open file is 2, as 0 and 1 are
     reserved for input and output, respectively. */
  t->next_file_descriptor = 2;

  if (thread_mlfqs) {

    /* Set initial threads niceness and recent_cpu to 0. */
    if (t == initial_thread) {
      t->nice = 0;
      t->recent_cpu = 0;

    } else {
      /* Inherit parent's niceness and recent CPU */
      t->nice = thread_get_nice();
      t->recent_cpu = thread_current()->recent_cpu;
    }
  }

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if(thread_mlfqs){

    int highest_priority = highest_ready_priority();

    if(highest_priority >= 0) {
      return list_entry(list_pop_back(&ready_lists_bsd[highest_priority]), 
                                      struct thread, 
                                      elem);
    } else {
      return idle_thread;
    }

  } else {
    if (list_empty (&ready_list)){
      return idle_thread;
    } else {
      return list_entry(list_pop_back(&ready_list), struct thread, elem);
    }
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Returns true if t is the idle thread */
bool
is_idle_thread(struct thread *t){
  return t==idle_thread;
}
