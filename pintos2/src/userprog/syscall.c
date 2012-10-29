#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "userprog/process.h"
#include "devices/shutdown.h"
#include <console.h>

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
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  int syscall_num = *((int *)(f->esp));
  int ret = syscall_vec[syscall_num](*((int *)(f->esp) + 1),
                                     *((int *)(f->esp) + 2),
                                     *((int *)(f->esp) + 3));

  f->eax = ret; 

 // thread_exit();
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

}

static int 
fork (void)
{
}

static int 
exec (const char *cmd_line)
{
}

static int 
dup2 (int old_fd, int new_fd)
{
}

static int 
pipe (int pipe[2])
{
}

static int 
wait (int pid)
{
}

static int 
open (const char *file)
{
}

static int 
filesize (int fd)
{
}

static int 
read (int fd, void *buffer, unsigned size)
{
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
}

static void 
close (int fd)
{
}
