#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

static struct process_info* parse_filename_and_args(
                              const char* file_name_and_args);
static void push_arguments_on_stack(struct process_info *process_to_start, 
                                    void **esp);
static void put_string_in_stack(void **esp, char *string);
static void put_uint_in_stack(void **esp, uint32_t n);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name_and_args) 
{
  ASSERT(strlen(file_name_and_args) > 0);

  tid_t tid;

  /* Calling helper method to parse arguments and set up process_info struct */
  struct process_info *process = parse_filename_and_args(file_name_and_args);

  /* In case of an error in the argument parsing */
  if (process == NULL) {
    return TID_ERROR;
  }

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (process->filename, PRI_DEFAULT, start_process, process);

  /* If thread could not be created, free the page allocated for the
     process. */
  if (tid == TID_ERROR) {
    palloc_free_page (process);
  }

  return tid;

}

/* Takes a string of arguments and returns a struct process_info with the
   filename, argv and argc fields all set */
static struct process_info*
parse_filename_and_args (const char* file_name_and_args)
{
  /* Allocating memory for our process_info struct */
  void *new_page = palloc_get_page(0);
  struct process_info *p = new_page;

  /* Create copy of file_name_and_args const string as strtok_r needs modifiable 
     string for tokenising */
  int arg_length = strlen(file_name_and_args) + 1;
  void *starting_address = new_page + PGSIZE - arg_length;


  /* Checking the arguments are small enough to fit into the page */
  if ((uint32_t) arg_length > (PGSIZE - sizeof(struct process_info))) {
    palloc_free_page(new_page);
    return NULL;
  }

  memcpy(starting_address, file_name_and_args, arg_length);

  /* Declaring helper pointers for strtok_r method */
  char *save_ptr;
  char *token;

  /* Setting number of arguments to 0, will be updated in the for loop */
  p->argc = 0;

  /* Tokenising the arguments and setting them in the process_info struct */
  for (char *name_args_copy = starting_address; ; name_args_copy = NULL) 
  {
    token = strtok_r(name_args_copy, " ", &save_ptr);

    if (token == NULL) 
      break;

    p->argv[p->argc] = token;
    p->argc++;

    if ((uintptr_t) &p->argv[p->argc] + sizeof(uint32_t)
        >= (uintptr_t) starting_address) {
      palloc_free_page(new_page);
      return NULL;
    }
  }

  /* File name is the first token in argv */
  p->filename = p->argv[0];
  ASSERT(strlen(p->filename) <= MAX_FILENAME_LENGTH);

  return p;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *process)
{

  struct process_info *process_to_start = (struct process_info *)process;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (process_to_start->filename, &if_.eip, &if_.esp);

  struct thread *cur = thread_current();

  /* Set thread's executable file to the file that was loaded, if it was
     indeed an executable file. */
  if (success) {
    strlcpy(cur->executable, process_to_start->filename,
                      strlen(process_to_start->filename) + 1);
  }

  /* Set the current thread's 'loaded' member to the return value from load,
     and then call sema_up on the thread's load_sema, so that sys_exec knows
     to now check whether the process has loaded successfully or not. */
  cur->loaded = success;
  sema_up(&cur->load_sema);

  /* Push the process arguments on to the stack using a helper method, if
     the process successfully loaded. */
  if (success) {
    push_arguments_on_stack(process_to_start, &if_.esp);
  }

  /* No longer need this page, as all required information is on the stack. */
  palloc_free_page (process_to_start);

  /* If load failed, quit. */
  if (!success) {
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Helper method which pushes a process' arguments onto the stack 
   given a process_info struct. */
static void 
push_arguments_on_stack(struct process_info *p, void **esp)
{
  /* Pushing arguments to the stack in reverse order */
  for (int j = p->argc; j > 0; j--) {
    put_string_in_stack(esp, p->argv[j-1]);
    p->argv[j-1] = *esp;
  }

  /* Rounding down the stack pointer to the nearest 
     word multiple for faster access */
  while (((uint32_t) *esp) % sizeof(uint32_t) != 0) {
    (*esp)--;
  } 

  /* Pushing null pointer sentinel */
  put_uint_in_stack(esp, 0);

  /* Pushing pointers to the arguments in reverse order */
  for (int k = p->argc; k > 0; k--) {
    put_uint_in_stack(esp, (uint32_t) p->argv[k-1]);
  }

  /* Pushing pointer to the first pointer */
  put_uint_in_stack(esp, (uint32_t) *esp);


  /* Pushing number of arguments */
  put_uint_in_stack(esp, p->argc);

  /* Pushing fake return address */
  put_uint_in_stack(esp, 0);

  // TODO: Use hex_dump() to test this is working!
  // hex_dump(if_.esp,if_.esp,size,true);
}

/* Pushes a string onto the stack at the next location given by the stack ptr */
static void
put_string_in_stack (void **esp, char *string)
{
  int str_length = strlen(string) + 1;
  *esp -= str_length;
  memcpy(*esp, string, str_length);
}

/* Pushes a uint32_t on the stack at the next location given by the stack ptr */
static void
put_uint_in_stack (void **esp, uint32_t n) 
{
  *esp -= sizeof(uint32_t);
  *((uint32_t*) *esp) = n;
}


/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting. */
int
process_wait (tid_t child_tid)
{
  struct thread *cur = thread_current();
  /* Check that the given tid is indeed a child of the current thread. */
  struct thread *child = NULL;
  struct list *children = &cur->children;
  struct list_elem *e;
  for (e = list_begin(children); e != list_end(children); e = list_next(e)) {
    struct thread *t = list_entry(e, struct thread, child_elem);
    if (t->tid == child_tid) {
      child = t;
      break;
    }
  }

  //TODO: Not sure if there will be a problem in main when process_wait is
  //      called because of never returning.
  /* If pid does not refer to a direct child of the calling process, -1
     is returned. -1 is also returned if wait has already been called on
     this thread. */
  if (child == NULL || child->waited_on) {
    return -1;
  }

  /* Don't need to set to false again, because the thread will be
     terminated once it is no longer being waited on by the parent,
     and in this case, we do not want to be able to call wait on this
     thread. */
  child->waited_on = true;

  /* Wait for child to terminate, then return it's exit status.
     Instead of a busy wait, give each thread a semaphore, initialised
     to 0 on initialising thread, then try to do sema_down here, which
     will wait until sema_up is called (in thread_exit), and then we
     can just return exit status. */
  sema_down(&child->exit_sema);

  /* Don't remove from the terminated child from the parents list of children,
     because we still want to be able to check to see if we have already called
     wait on this child. */

  /* Have changed page_fault() to set exit status to -1, so we can
     still just return exit_status in the case that the process was
     terminated by the kernel. */
  //TODO: Do all kernel terminations incur a page fault???
  return child->exit_status;

}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Close all files that this thread had open. */
  while(!list_empty(&cur->files)) {
    struct list_elem* e = list_begin(&cur->files);
    struct proc_file* f = list_entry(e, struct proc_file, file_elem);
    file_close(f->file);
    list_remove(&f->file_elem);
    free(f);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
//  // ADDED
//  file_deny_write(file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success) 
      {
        *esp = PHYS_BASE;
      }
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
