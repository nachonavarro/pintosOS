#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "threads/malloc.h"
#include "lib/string.h"

/* Ensures multiple threads cannot call file system code at the same time. */
struct lock secure_file;

static void syscall_handler (struct intr_frame *);
static void sys_halt(void);
static pid_t sys_exec(const char *cmd_line);
static int sys_wait(pid_t pid);
static bool sys_create(const char *file, unsigned initial_size);
static bool sys_remove(const char *file);
static int sys_open(const char *file);
static int sys_filesize(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_seek(int fd, unsigned position);
static unsigned sys_tell(int fd);
static void sys_close(int fd);
static mapid_t sys_mmap(int fd, void *addr);
static void sys_munmap(mapid_t mapping);

/* Helper functions for system calls. */
static struct file* get_file(int fd);
static void check_mem_ptr(const void *uaddr);
static void check_fd(int fd);
static uint32_t get_word_on_stack(struct intr_frame *f, int offset);
static void check_buffer(const void *buffer, unsigned size);
static bool is_executable(const char *file);

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

  int syscall_number = (int)get_word_on_stack(f, 0);

  switch(syscall_number) {
    case SYS_HALT:
    {
      sys_halt();
      break;
    }
    case SYS_EXIT:
    {
      int status = (int)get_word_on_stack(f, 1);
      sys_exit(status);
      /* Returns exit status to the kernel. */
      f->eax = status;
      break;
    }
    case SYS_EXEC:
    {
      const char *cmd_line  = (const char *)get_word_on_stack(f, 1);
      pid_t pid = sys_exec(cmd_line);
      /* Returns new processes pid. */
      f->eax = pid;
      break;
    }
    case SYS_WAIT:
    {
      pid_t pid = (pid_t)get_word_on_stack(f, 1);
      /* Returns child's exit status (pid argument is pid of this child). */
      f->eax = sys_wait(pid);
      break;
    }
    case SYS_CREATE:
    {
      const char *filename  = (const char *)get_word_on_stack(f, 1);
      unsigned initial_size = (unsigned)get_word_on_stack(f, 2);
      /* Returns true to the kernel if creation is successful. */
      f->eax = (int)sys_create(filename, initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      const char *filename = (const char *)get_word_on_stack(f, 1);
      /* Returns true if successful, and false otherwise. */
      f->eax = sys_remove(filename);
      break;
    }
    case SYS_OPEN:
    {
      const char *filename = (const char *)get_word_on_stack(f, 1);
      /* Returns file descriptor of opened file, or -1 if it could not
         be opened. */
      f->eax = sys_open(filename);
      break;
    }
    case SYS_FILESIZE:
    {
      int fd = (int)get_word_on_stack(f, 1);
      /* Returns size of file in bytes. */
      f->eax = sys_filesize(fd);
      break;
    }
    case SYS_READ:
    {
      int fd        = (int)get_word_on_stack(f, 1);
      void *buffer  = (void *)get_word_on_stack(f, 2);
      unsigned size = (unsigned)get_word_on_stack(f, 3);
      /* Returns number of bytes actually read, or -1 if it could not
         be read. */
      f->eax = sys_read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd        = (int)get_word_on_stack(f, 1);
      void *buffer  = (void *)get_word_on_stack(f, 2);
      unsigned size = (unsigned)get_word_on_stack(f, 3);
      /* Returns number of bytes written. */
      f->eax = sys_write(fd, buffer, size);
      break;
    }
    case SYS_SEEK:
    {
      int fd             = (int)get_word_on_stack(f, 1);
      unsigned position  = (int)get_word_on_stack(f, 2);
      sys_seek(fd, position);
      break;
    }
    case SYS_TELL:
    {
      int fd        = (int)get_word_on_stack(f, 1);
      /* Returns the position of the next byte to be read or written in open
         file 'fd' (in bytes, from start of file). */
      f->eax = sys_tell(fd);
      break;
    }
    case SYS_CLOSE:
    {
      int fd        = (int)get_word_on_stack(f, 1);
      sys_close(fd);
      break;
    }
    case SYS_MMAP:
    {
        int fd     = (int)get_word_on_stack(f, 1);
        void *addr = (void *)get_word_on_stack(f, 2);
        //f->eax = sys_mmap(fd, addr);
        break;
    }
    case SYS_MUNMAP:
    {
        mapid_t mapping = (mapid_t)get_word_on_stack(f, 1);
        sys_munmap(mapping);
        break;
    }
    default:
    {
      NOT_REACHED();
    }
  }

}

/* Terminates Pintos by calling shutdown_power_off().
   Should rarely be used, as some information is lost
   (such as information about possible deadlocks). */
static void
sys_halt(void) 
{
  shutdown_power_off();
}

/* Terminates the current user program. If the processes parent waits for it,
   this status will be returned to the parent (Parent looks at child's
   exit status). Status of 0 indicates success, and anything else indicates
   an error. After this function is called in syscall_handler(), the exit
   status is sent ('returned') to the kernel. */
void
sys_exit(int status) 
{
  struct thread *cur = thread_current();
  cur->exit_status = status;
  /* Process termination message, printing process' name and exit status. */
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

/* Runs the executable given in cmd_line. cmd_line includes the arguments as
   well. Returns the new process' pid. Returns -1 if the process could not
   load or run for some reason. After this function is called in
   syscall_handler(), the new process' id is sent to the kernel. Parent/Child
   relationship is set in process_execute(). */
static pid_t
sys_exec(const char *cmd_line) 
{
  check_mem_ptr(cmd_line);
  lock_acquire(&secure_file);

  /* Identity mapping between thread id and process id, because
     Pintos is not multithreaded. */
  pid_t pid = (pid_t)process_execute(cmd_line);

  /* We want to wait until the child has definitely loaded, and then check to
     see whether it has loaded or not (e.g. whether the filename was invalid).
     load_sema is initialised to 0 for a thread. We use load_sema of the child
     to make this function wait, by doing sema_down on the child's exec_sema. We
     will have to wait until sema_up has been called on this sema in
     start_process (after load has been called). Just before sema_up has been
     called, start_process will set the threads loaded bool to true, so we can
     then read this bool to decide whether to return -1.  */
  enum intr_level old_level = intr_disable();
  struct thread *child = tid_to_thread((tid_t)pid);
  intr_set_level(old_level);

  sema_down(&child->load_sema);
  lock_release(&secure_file);

  if (!(child->loaded))
    return PID_ERROR;

  return pid;
}

/* Waits for a child process pid, and then returns the child's exit status.
   See process_wait() for more information on what exactly happens here. */
static int
sys_wait(pid_t pid) 
{
  return process_wait((tid_t) pid);
}

/* Creates file called 'file' that is initially 'initial_size' bytes.
   Returns true if successful. Creating a file does NOT open it. */
static bool
sys_create(const char *file, unsigned initial_size) 
{
  check_mem_ptr(file);

  if (file == NULL)
    sys_exit(ERROR);

  lock_acquire(&secure_file);
  bool success = filesys_create(file, initial_size);
  lock_release(&secure_file);

  return success;
}

/* Removes file called 'file'. Returns true if successful, and false otherwise
   (including when no file named 'file' exists). An open file can be removed,
   but it is not closed. */
static bool
sys_remove(const char *file) 
{
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
sys_open(const char *file) 
{
  check_mem_ptr(file);

  if (file == NULL)
    sys_exit(ERROR);

  lock_acquire(&secure_file);

  struct file *fl = filesys_open(file);
  if (fl == NULL) 
  {
    lock_release(&secure_file);
    return FD_ERROR;
  }

  struct thread *t = thread_current();
  /* Freed in sys_close(). */
  struct proc_file *f = malloc(sizeof(struct proc_file));

  if (f == NULL)
    sys_exit(FD_ERROR);

  list_push_front(&t->files, &f->file_elem);
  f->file = fl;
  /* If file is currently being run as an executable in this process, we must
     not be able to write to it. */
  if (is_executable(file))
    file_deny_write(f->file);

  int file_descriptor = t->next_file_descriptor;
  f->fd = file_descriptor;
  /* Increment next_file_descriptor so that the next file to be
     opened has a different file descriptor. */
  t->next_file_descriptor++;

  lock_release(&secure_file);

  return file_descriptor;
}

/* Returns size, in bytes, of file open as 'fd'. */
static int
sys_filesize(int fd) 
{
  check_fd(fd);

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
sys_read(int fd, void *buffer, unsigned size) 
{
  /* Cannot read from stdout. */
  if (fd == STDOUT_FILENO)
    sys_exit(ERROR);

  check_fd(fd);
  check_buffer(buffer, size);

  int bytes;

  lock_acquire(&secure_file);

  /* fd = 0 corresponds to reading from stdin. */
  if (fd == STDIN_FILENO) 
  {
    unsigned i;
    uint8_t keys[size];
    /* Make an array of keys pressed. */
    for (i = 0; i < size; i++) 
    {
      keys[i] = input_getc();
    }
    /* Put these keys pressed into the buffer. */
    memcpy(buffer, (const void *) keys, size);
    /* Must have successfully read all bytes we were told to. */
    bytes = size;
  } else {

    struct file *f = get_file(fd);
    if (!f) 
    {
      lock_release(&secure_file);
      return ERROR;
    }
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
sys_write(int fd, const void *buffer, unsigned size) 
{

  if (fd == STDIN_FILENO)
    sys_exit(ERROR);

  check_mem_ptr(buffer);
  check_fd(fd);
  check_buffer(buffer, size);

  int bytes;
  lock_acquire(&secure_file);

  /* fd = 1 corresponds to writing to stdout. */
  if (fd == STDOUT_FILENO)
  {
    /* If we are writing a fairly large amount of bytes to stdout, write
       MAX_CONSOLE_WRITE bytes per call to putbuf(), then write the rest
       of the bytes, calling sys_write() recursively. */
    if (size < MAX_CONSOLE_WRITE) {
      putbuf(buffer, size);

    } else {
      putbuf(buffer, MAX_CONSOLE_WRITE);
      sys_write(fd, buffer + MAX_CONSOLE_WRITE, size - MAX_CONSOLE_WRITE);
    }
    /* Must have successfully written all bytes we were told to. */
    bytes = size;
  } 
    else 
  {
    struct file *f = get_file(fd);
    if (f == NULL) 
    {
      lock_release(&secure_file);
      return ERROR;
    }
    bytes = file_write(f, buffer, size);
  }

  lock_release(&secure_file);

  return bytes;

}

/* Changes the next byte to be read or written in open file 'fd' to
   'position' (in bytes, from start of file). */
static void
sys_seek(int fd, unsigned position) 
{
  check_fd(fd);

  lock_acquire(&secure_file);
  struct file *f = get_file(fd);

  if (f == NULL) 
  {
    lock_release(&secure_file);
    return;
  }

  file_seek(f, position);
  lock_release(&secure_file);
}

/* Returns the position of the next byte to be read or written in open
   file 'fd' (in bytes, from start of file). */
static unsigned
sys_tell(int fd) 
{
  check_fd(fd);

  lock_acquire(&secure_file);
  struct file *f = get_file(fd);

  if (f == NULL) 
  {
    lock_release(&secure_file);
    sys_exit(ERROR);
  }

  int position = file_tell(f);
  lock_release(&secure_file);

  return position;
}

/* Closes file descriptor fd. */
static void
sys_close(int fd) 
{
  check_fd(fd);
  lock_acquire(&secure_file);
  struct thread *cur = thread_current();
  struct list_elem *e;

  /* Cannot use get_file() in place of the below for loop, because we need
     access to the file_elem, which is in the struct proc_file, not the
     struct file. */
  for (e = list_begin (&cur->files); 
       e != list_end (&cur->files);
       e = list_next (e)) 
  {
    struct proc_file *f = list_entry(e, struct proc_file, file_elem);
    if (fd == f->fd) 
    {
      file_close(f->file);
      list_remove(&f->file_elem);
      free(f);
      break;
    }
  }
  lock_release(&secure_file);
}

//TODO: "Your VM system must lazily load pages in mmap regions"
/* Maps the file open as FD into the process' virtual address space - entire
   file mapped into consecutive virtual pages starting at ADDR. (Lazy load
   pages in mmap regions). (Evicting a page mapped by mmap writes it back to
   the actual file it was mapped from). (Set spare bytes on final page to zero
   when that page is faulted in the file system, and ignore these bytes when
   page is written back to disk). Returns mapid_t for the mapping, or -1 on
   failure. Failure occurs when file has length 0, if addr is not page aligned,
   if range of pages mapped overlaps any existing mapped pages (including the
   stack or pages mapped at executable load time), if addr is 0, or if fd
   is 0 or 1. */
static mapid_t
sys_mmap(int fd, void *addr)
{
  /* Check that fd is a valid file descriptor. */
  check_fd(fd);

  /* Cannot map stdin or stdout. */
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    return ERROR;
  }

  /* Address to map to cannot be 0, because some Pintos code assumes virtual
     page 0 is not mapped. */
  if (addr == 0) {
    return ERROR;
  }

  int size = sys_filesize(fd);

  /* Cannot map a file of size 0 bytes. */
  if (size == 0) {
    return ERROR;
  }

  /* ADDR must be page-aligned. */
  if (pg_ofs(addr) != 0) {
    return ERROR;
  }

  //TODO: Still need to 'return ERROR' if range of pages mapped overlaps any
  //      existing mapped pages.
  /* num_stack_pages is number of pages for the stack for this process, I
     think. However, could probably use the esp of the current thread, and then
     simply compare this with ADDR
     (Want ADDR < esp OR ADDR < PHYS_BASE - num_stack_pages*PGSIZE) */
  int num_stack_pages = 0; //TODO: Change from 0
  if (addr > PHYS_BASE - (num_stack_pages*PGSIZE)) {
    return ERROR;
  }

  /* If ADDR is not in user/process address space, we cannot map
       the file there. */
  if (!is_user_vaddr(addr)) {
    sys_exit(ERROR); //TODO: Not sure whether to return ERROR instead
  }

  /* Pages is number of pages needed to map file.
     (size % PGSIZE) gives the number of spare bytes on the final page.
     This is necessary because the division is integer division, and so
     will round down, but we want it to round up. */
  int pages = size / PGSIZE;
  if ((size % PGSIZE) != 0) {
    pages++;
  }

  //TODO: Use supplemental page table, potentially, to check if pages needed
  //      overlaps any existing mapped pages
  /* Check that the contiguous range of pages to be mapped doesn't overlap
     any existing set of mapped pages (Not including stack). */
  int i;
  for (i = 0; i < pages; i++) {
    //TODO: Check addr + (i * PGSIZE) in supplementary page table here?
    //      Return false if any entry in this table exists
  }

  //TODO: Insert all new pages into supplementary page table???? Is this
  //      the lazy thing??? I thought lazy means only when we try to access
  //      it and page fault, then we check to see if we have it, then try
  //      to load it
  //      THIS MAY BE WHAT CAUSES THE PAGES TO BE ADDED TO THE PROCESS' LIST OF
  //      VIRTUAL PAGES!!

  lock_acquire(&secure_file);
  struct file *old_file = get_file(fd);
  if (!old_file) {
    lock_release(&secure_file);
    sys_exit(ERROR);
  }
  /* Must use file_reopen() to get independent 'struct file *' for same file
     (with same inode) because the file could be being read at different points
     (file->pos could be different) and they could have different
     file->deny_write (file_deny_write() could be called on one struct file but
     not another of same file (inode) but different struct file). */
  struct file *file = file_reopen(old_file);
  lock_release(&secure_file);

  struct thread *cur = thread_current();

  mapid_t mapid = cur->next_mapid;

  /* Lock must be acquired to call hash_insert() in mmap_table_insert(), and
     since we have thread_current() here already it makes sense to lock here
     rather than in mmap_table_insert() in mmap.c. */
  lock_acquire(&cur->mmap_table_lock);
  bool success = mmap_table_insert(&cur->mmap_table, addr, addr + size, pages, mapid, file);
  lock_release(&cur->mmap_table_lock);

  /* Return -1 if mmap_table_insert wasn't successful (meaning there isn't
     enough space to malloc for a struct mmap_mapping *). */
  if (!success) {
    return ERROR;
  }

  /* Increment next_mapid for this thread, so that the next mmap will have a
     different mapid, ensuring unique mapids for all mappings for a process.
     Increment after checking for mmap_table_insert() success status, because
     in the case of failure, we can reuse the mapid that the failed mapping
     would have had. */
  cur->next_mapid++;

  /* If successful, function returns the mapid that uniquely identifies
     the mapping within the process. */
  return mapid;
}

//TODO: Delete this comment.
/* ***STEPS IN A MUNMAP***
 *
 * *Each pages written to need to be written back to file (Dirty bit!)
 *
 * *Remove each page from spt
 *
 * *hash_delete() and free(struct mmap_mapping) (in mmap_mapping_delete, but
 *  this needs to be changed, because we now need to mmap_mapping_lookup in
 *  sys_munmap)                                                              */

/* Unmaps the mapping for current thread with mapid MAPPING. Each page that
   has been wriiten to by the process needs to be written back to the file.
   Each page must be removed from the process' list of virtual pages. */
static void
sys_munmap(mapid_t mapping)
{
  struct thread *cur = thread_current();
  struct hash *spt = &cur->supp_pt;
  struct hash *mmap_table = &cur->mmap_table;
  struct mmap_mapping *mmap = mmap_mapping_lookup(mmap_table, mapping);

  /* MAPPING must be a mapping ID returned by a previous call to sys_mmap() by
     the same process that has not yet been unmapped. */
  if (mmap == NULL) {
    sys_exit(ERROR);
  }

  void *page_uaddr = mmap->start_uaddr;
  int num_pages = mmap->num_pages;
  uint32_t *pd = cur->pagedir;
  struct file *file = mmap->file;

  int bytes_to_write;
  int i;

  /* Need to remove each page from the process' list of virtual pages
     (Supplemental page table). Also, each page that has been written to by the
     process must be written back to the file. */
  for (i = 0; i < num_pages; i++) {
    /* If dirty bit is set for this page, it has been written to. */
    if (pagedir_is_dirty(pd, page_uaddr)) {

      /* If we are on the last page, we don't want to write the last bytes on
         this page (that 'stick out' beyond the end of file) back to the file.
         This happens when file's length is not a multiple of PGSIZE, but even
         in the case where the file size is a multiple of PGSIZE, both
         outcomes of the below statement will set bytes_to_read to PGSIZE
         (Because then end_uaddr - page_uaddr = PGSIZE). */
      bytes_to_write = (i == (num_pages - 1)) ? (mmap->end_uaddr - page_uaddr)
                                              : PGSIZE;

      /* Must acquire lock when calling file system code. */
      lock_acquire(&secure_file);
      /* Ensure we write to correct position in file by using file_seek. */
      file_seek(file, ((off_t) page_uaddr) - mmap->start_uaddr);
      /* Write this page back to the file, as this page has been written to. */
      file_write(file, page_uaddr, bytes_to_write);
      lock_release(&secure_file);
    }

    //TODO: spt_remove(page from spt)

    /* Advance to the next page. */
    page_uaddr += PGSIZE;
  }

  /* Lock must be acquired to call hash_delete() in mmap_mapping_delete(). */
  lock_acquire(&cur->mmap_table_lock);
  /* Remove the mapping MMAP from MMAP_TABLE and free MMAP. */
  mmap_mapping_delete(mmap_table, mmap);
  lock_release(&cur->mmap_table_lock);
}

void
munmap_from_hash_elem(struct hash_elem *e, void *aux UNUSED) {
  struct mmap_mapping *mmap = hash_entry(e, struct mmap_mapping, hash_elem);
  sys_munmap(mmap->mapid);
}

/* Returns the file corresponding the to supplied file descriptor
   in the current thread's list of files that it can see. */
static struct file*
get_file(int fd)
{
  struct thread *cur = thread_current();
  struct list_elem *e;

  for (e = list_begin (&cur->files); 
       e != list_end (&cur->files);
       e = list_next (e)) 
  {
    struct proc_file *f = list_entry(e, struct proc_file, file_elem);
    if (fd == f->fd) 
      return f->file;
  }
  NOT_REACHED();
  return NULL;
}

/* Returns the word (4 bytes) at a given offset from a frames stack pointer.
   Only aligned word access is possible because the stack pointer is cast
   from a (void *) to a (uint32_t *). */
static uint32_t 
get_word_on_stack(struct intr_frame *f, int offset) 
{
  uint32_t *esp = f->esp;
  check_mem_ptr(esp + offset);
  return *(esp + offset);
}

/* If supplied pointer is a null pointer, not in the user address space, or
   is an unmapped virtual address, the process is terminated. */
static void
check_mem_ptr(const void *uaddr) 
{
  if (uaddr == NULL || !is_user_vaddr(uaddr)
      || pagedir_get_page(thread_current()->pagedir, uaddr) == NULL)
    sys_exit(ERROR);
}

/* Checks that all of the buffer that we are writing/reading from is valid. */
static void
check_buffer(const void *buffer, unsigned size)
{
  char *buf = (char *) buffer;
  unsigned i;
  for (i = 0; i < size; i++) 
  {
    check_mem_ptr(buf);
    buf++;
  }
}

/* Checks that the given file descriptor is valid. File descriptors cannot
   be less than 0. */
static void
check_fd(int fd) 
{
  struct thread *cur = thread_current();
  int next_fd = cur->next_file_descriptor;

  if (fd < 0 || fd >= next_fd)
    sys_exit(ERROR);
}

/* Returns true if the supplied filename is the executable running in the
   current thread/process. */
static bool
is_executable(const char *file) 
{
  return (strcmp(file, thread_current()->executable) == 0);
}
