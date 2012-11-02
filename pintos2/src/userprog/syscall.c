#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include <console.h>

#include "filesys/filesys.h"
#include "userprog/pagedir.h"

#define NUM_SYSCALLS 32
#define MAX_FD 128 /* maximum number of FDs a thread can have open */

static void syscall_handler (struct intr_frame *);

typedef void *(*handler) (void *arg1, void *arg2, void *arg3);

static handler syscall_vec[NUM_SYSCALLS];

static void halt (void);
static int  fork (void);
static int exec (const char *cmd_line);
static int dup2 (int old_fd, int new_fd);
static int pipe (int pipe[2]);
static int wait (int pid);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static unsigned tell (int fd);
static void close (int fd);
static int create (const char *file, unsigned initial_size);

static bool verify_ptr(void *ptr);
static bool verify_fd(int fd);

void
syscall_init (void) 
{
  //initialize syscall_vec
  syscall_vec[SYS_HALT] = (handler)halt;
  syscall_vec[SYS_EXIT] = (handler)exit;
  syscall_vec[SYS_FORK] = (handler)fork;
  syscall_vec[SYS_EXEC] = (handler)exec;
  syscall_vec[SYS_DUP2] = (handler)dup2;
  syscall_vec[SYS_PIPE] = (handler)pipe;
  syscall_vec[SYS_WAIT] = (handler)wait;
  syscall_vec[SYS_OPEN] = (handler)open;
  syscall_vec[SYS_FILESIZE] = (handler)filesize;
  syscall_vec[SYS_READ] = (handler)read;
  syscall_vec[SYS_WRITE] = (handler)write;
  syscall_vec[SYS_TELL] = (handler)tell;
  syscall_vec[SYS_CLOSE] = (handler)close;
  syscall_vec[SYS_CREATE] = (handler)create;

  lock_init (&filesys_lock);

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{

  int *sp = f->esp;

  //Test valid stack pointer
  
  if (!verify_ptr(sp))
      exit(-1);
  
  int syscall_num = *sp;
  //Test valid syscall num
  if (syscall_num < 0 || syscall_num >= NUM_SYSCALLS)
      exit(-1);

  //Test valid arguments
  if (!verify_ptr (sp + 1) ||
      !verify_ptr (sp + 2) ||
      !verify_ptr (sp + 3))
    {
      exit(-1);
    }
      

  int ret = syscall_vec[syscall_num](*((sp) + 1),
                                     *((sp) + 2),
                                     *((sp) + 3));

  f->eax = ret; 
}


static void 
halt (void) 
{
  shutdown_power_off();
}


void 
exit (int status)
{
  struct thread *t = thread_current();
  t->return_status = status;

  printf ("%s: exit(%d)\n", t->name, t->return_status);

  thread_exit();
}

static int 
fork (void)
{
  int i;
  struct thread *parent = thread_current();
  tid_t child_tid = thread_create (parent->name, PRI_DEFAULT, process_fork, NULL);
  struct thread *child = thread_from_tid(child_tid);

  child->pagedir = pagedir_create();
  //copy over each page write function to loop through pages
  //memcpy(child->pagedir, parent->pagedir, PGSIZE);
  pagedir_dup (child->pagedir, parent->pagedir);

  //copy over fds 
  for (i = 0; i < MAX_FD; i++) 
  {
    if (parent->fds[i] != NULL)
    {
      child->fds[i] = file_reopen(parent->fds[i]);
      file_seek (child->fds[i], file_tell (parent->fds[i]));
      if (get_deny_write (parent->fds[i]))
      {
        file_deny_write (child->fds[i]);
      }
    }
  }

  sema_up(&child->wait_sema);

  return child_tid;
}

static int 
exec (const char *cmd_line)
{
  if(!verify_ptr(cmd_line))
    exit(-1);

  tid_t tid = process_execute(cmd_line);
  if (tid == TID_ERROR)
    return -1;
  return tid;
}

static int 
dup2 (int old_fd, int new_fd)
{
/*
 * It looks like this implementation should work but I don't see any
 * tests that test this syscall???
 *
 * After this function is called, there are multiple FDs pointing to the 
 * same file. Not sure if we have to worry about what happens when one
 * of the FDs call close on it's file, while another FD is still trying
 * to use the file. 
 */

  if(old_fd == new_fd)
    return -1;

  if(!verify_fd(old_fd) || !verify_fd(new_fd))
    return -1;

  struct file **fds = thread_current()->fds;
  struct file *old_file = fds[old_fd];

  if(old_file == NULL)
    return -1;

  fds[new_fd] = old_file;

  return new_fd;
}

static int 
pipe (int pipe[2])
{
  if(pipe == NULL)
    return -1;

  int fd0 = pipe[0];
  int fd1 = pipe[1];

  if(!verify_fd(fd0) || !verify_fd(fd1))
    return -1;

  struct file **fds = thread_current()->fds;
  struct file *read_file = fds[fd0];

  if(read_file == NULL)
    return -1;
  //Make write_end's FD the FD that read_end uses
  fds[fd1] = read_file;

  //on success, return 0
  return 0;
}

static int 
wait (int pid)
{
  return process_wait(pid); 
}

static int 
open (const char *file)
{
  if (!verify_ptr(file))
    exit(-1);
 

  lock_acquire(&filesys_lock);
  //read size bytes from fd(file) into buffer
  struct file **fds = thread_current()->fds;
  int index = 3; //FD 0, 1, 2 are reserver for std in, out, err
  bool foundOpenFD = false;
  //traverse FD list to find lowest avaliable FD
  while(!foundOpenFD)
  {
    if(index == MAX_FD) 
    {
      //we ran out of FDs
      lock_release(&filesys_lock);
      return -1;
    }
    if(fds[index] == NULL)
    {
      //we found an avaliable FD
      foundOpenFD = true;
      struct file *opened = filesys_open(file);
      if(opened == NULL)
      {
        //not a file
         lock_release(&filesys_lock);
         return -1;
      }

      fds[index] = opened;
    } 
    else
    {
      //FD is in use, check the next one
      index++;
    }
  }

  lock_release(&filesys_lock);
  return index;
}

static int 
filesize (int fd)
{
  if (!verify_fd(fd))
    return -1;

  struct file *file = thread_current()->fds[fd];
  if (file == NULL)
    return -1;

  lock_acquire(&filesys_lock);
  int length = file_length (file);
  lock_release(&filesys_lock);

  return length;
}

static int 
read (int fd, void *buffer, unsigned size)
{
  int i;

  if (!verify_ptr(buffer) || !verify_ptr(buffer + size))
    exit(-1);

  //check valid FD
  if(!verify_fd(fd))
    return -1;


  lock_acquire(&filesys_lock);
  if (fd == 0) 
  {
    for (i = 0; i < size; i++)
      *(char *)(buffer + i) = input_getc();
    lock_release(&filesys_lock);
    return size;
  }

  if (fd == 1)
  {
    lock_release(&filesys_lock);
    return -1;
  }

  struct file *file = (thread_current()->fds)[fd];
  if(file == NULL)
  {
    //this FD is not in use right now
    lock_release(&filesys_lock);
    return -1;
  }


  int bytes_read = file_read(file, buffer, size);
  lock_release(&filesys_lock);
  return bytes_read;
}

static int 
write (int fd, const void *buffer, unsigned size)
{
  if (!verify_ptr(buffer) || !verify_ptr(buffer + size))
    exit(-1);

  //check for valid FD number
  if(!verify_fd(fd))
    return -1;

  lock_acquire(&filesys_lock);
  if (fd == 0) 
  {
    lock_release(&filesys_lock);
    return -1;
  }

  if (fd == 1)
  {
    putbuf (buffer, size);
    lock_release(&filesys_lock);
    return size;
  }

  
  struct file *file = (thread_current()->fds)[fd];  
  //make sure FD points to an open file
  if(!file)
  {
    lock_release(&filesys_lock);
    return -1;
  }

  int bytes_written = file_write(file, buffer, size);
  lock_release(&filesys_lock);
  return bytes_written;
}

static unsigned 
tell (int fd)
{
  if (!verify_fd(fd))
    return -1;

  lock_acquire(&filesys_lock);
  int pos = file_tell(thread_current()->fds[fd]);
  lock_release(&filesys_lock);
  return pos;
}

static void 
close (int fd)
{
  if(verify_fd(fd))
  {
    struct file *closed = thread_current()->fds[fd];

    lock_acquire(&filesys_lock);
    if (closed != NULL)
    {
      thread_current()->fds[fd] = NULL;
      file_close (closed);
    }
    lock_release(&filesys_lock);
  }
}

static bool verify_ptr(void *ptr) {
  return is_user_vaddr(ptr) && pagedir_get_page(thread_current()->pagedir, ptr);
}

static int
create (const char *file, unsigned initial_size)
{
  if (!verify_ptr(file))
    exit(-1);

  return filesys_create (file, initial_size);
}


static bool verify_fd(int fd)
{
  if(fd < 0 || fd >= MAX_FD)
  {
    return false;
  }
  return true;
}
