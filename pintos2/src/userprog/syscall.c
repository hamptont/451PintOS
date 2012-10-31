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

#define NUM_SYSCALLS 32

static void syscall_handler (struct intr_frame *);

typedef void *(*handler) (void *arg1, void *arg2, void *arg3);

static handler syscall_vec[NUM_SYSCALLS];

static void halt (void);
static void exit (int status);
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

static bool verify_ptr(void *ptr);


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
  syscall_vec[SYS_FILESIZE] = (handler)open;
  syscall_vec[SYS_READ] = (handler)read;
  syscall_vec[SYS_WRITE] = (handler)write;
  syscall_vec[SYS_TELL] = (handler)tell;
  syscall_vec[SYS_CLOSE] = (handler)close;

  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{

  int *sp = f->esp;

  //Test valid stack pointer
  
  if (!verify_ptr(sp))
    {
      exit(-1);
      return;
    }
  
  int syscall_num = *sp;
  //Test valid syscall num
  if (syscall_num < 0 || syscall_num >= NUM_SYSCALLS)
    {
      exit(-1);
      return;
    }

  //Test valid arguments
  if (!verify_ptr (sp + 1) ||
      !verify_ptr (sp + 2) ||
      !verify_ptr (sp + 3))
    {
      exit(-1);
      return;
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


static void 
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
  return 0;
}

static int 
exec (const char *cmd_line)
{
  return 0;
}

static int 
dup2 (int old_fd, int new_fd)
{
  return 0;
}

static int 
pipe (int pipe[2])
{
  return 0;
}

static int 
wait (int pid)
{
  return process_wait(pid); 
}


/*
 * TODO: need to make sure *file is a valid address / pointer to a valid file
 * fails on open((char *) 0x20101234)) --> should call exit(-1)
 */
static int 
open (const char *file)
{
  if (!verify_ptr(file))
  {
    exit(-1);
  }
 

  if(file == NULL || *file == NULL)
  {
    //file does not exist
    return -1;
  }

  //read size bytes from fd(file) into buffer
  struct file *fds = thread_current()->fds;
  int index = 3; //FD 0, 1, 2 are reserver for std in, out, err
  bool foundOpenFD = false;
  //traverse FD list to find lowest avaliable FD
  while(!foundOpenFD)
  {
    if(index == 128) 
    {
      //we ran out of FDs
      return -1;
    }
    if(!fds[index].open)
    {
      //we found an avaliable FD
      foundOpenFD = true;
      struct file *opened = filesys_open(file);
      if(opened == NULL)
      {
        //not a file
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
  return index;
}

static int 
filesize (int fd)
{
  return 0;
}

static int 
read (int fd, void *buffer, unsigned size)
{
  //read size bytes from fd(file) into buffer
  //check valid FD
  if(fd < 0 || fd > 128)
  {
    return -1;
  }
  struct file file = (thread_current()->fds)[fd];
  if(!file.open)
  {
    //this FD is not in use right now
    return -1;
  }


  return 0;
}

static int 
write (int fd, const void *buffer, unsigned size)
{
  putbuf (buffer, size);

  return size;
}

static unsigned 
tell (int fd)
{
  return 0;
}

static void 
close (int fd)
{
  if(fd >= 0 && fd < 128)
  {
    thread_current()->fds[fd].open = false;
  }
}

static bool verify_ptr(void *ptr) {
  return is_user_vaddr(ptr) && pagedir_get_page(thread_current()->pagedir, ptr);
}
