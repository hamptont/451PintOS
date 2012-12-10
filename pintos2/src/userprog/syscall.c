#include "userprog/syscall.h"
//#include "lib/user/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "vm/page.h"
#include <console.h>

#include "filesys/filesys.h"
#include "userprog/pagedir.h"

#define NUM_SYSCALLS 32
static void syscall_handler (struct intr_frame *);

typedef void *(*handler) (void *arg1, void *arg2, void *arg3);

static handler syscall_vec[NUM_SYSCALLS];

typedef int mapid_t;

static void halt (void);
static int  fork (struct intr_frame *f);
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
static bool remove (const char *file_name);

static bool verify_ptr(void *ptr);
static bool verify_fd(int fd);

//mmap files
static mapid_t mmap(int fd, void *addr);
static void munmap(mapid_t mapping);

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
  syscall_vec[SYS_MMAP] = (handler)mmap;
  syscall_vec[SYS_MUNMAP] = (handler)munmap;
  syscall_vec[SYS_REMOVE] = (handler)remove;

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
     
  int ret;
  if (syscall_num == SYS_FORK) {
    ret = syscall_vec[SYS_FORK](f, NULL, NULL);
  } else {
    ret = syscall_vec[syscall_num](*((sp) + 1),
                                     *((sp) + 2),
                                     *((sp) + 3));
  }
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
fork (struct intr_frame *f)
{
  int i;
  struct thread *parent = thread_current();
  struct thread *child = create_child_thread ();

  setup_thread_to_return_from_fork (child, f);
  child->pagedir = pagedir_duplicate (parent->pagedir);

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

  list_push_back(&parent->child_list, &child->child_list_elem);
  child->parent = thread_current();

  thread_unblock (child);
  return child->tid;
}

static int 
exec (const char *cmd_line)
{
  if(!verify_ptr(cmd_line))
    exit(-1);

  tid_t tid = process_exec(cmd_line);
  if (tid == TID_ERROR)
    return -1;
  return tid;
}

static int 
dup2 (int old_fd, int new_fd)
{

  if(old_fd == new_fd)
    return -1;

  if(!verify_fd(old_fd) || !verify_fd(new_fd))
    return -1;

  struct file **fds = thread_current()->fds;
  struct file *old_file = fds[old_fd];

  if(old_file == NULL)
    return -1;

  //point the new FD to a copy of old_file
  struct file *new_file = file_reopen(old_file);

  //copy over offset from old file to new file
  off_t old_offset = file_tell(old_file);
  file_seek(new_file, old_offset);

  //copy over writeable bit from old file to new file
  bool old_wbit = get_deny_write(old_file);
  if(old_wbit)
  {
    file_allow_write(new_file);
  }
  else
  {
    file_deny_write(new_file);
  }

  //point new fd to new file
  fds[new_fd] = new_file;

  return new_fd;
}

static int 
pipe (int pipe[2])
{
  if(pipe == NULL)
  {
    //we ran out of FDs
    return -1;
  }

  bool foundOpenIndex = false;
  int index = 0;
  while(!foundOpenIndex)
  {
    if(index == MAX_PIPES)
    {
      return -1;
    }
    if(pipe_buffer_array[index] == NULL)
    {
      //found a spot to store our pipe
      foundOpenIndex = true;
      struct pipe_buffer *buffer = calloc(1, sizeof(struct pipe_buffer));
      if(buffer == NULL)
      {
        //calloc failed
        return -1;
      }
      buffer->start = 0;
      buffer->end = 0;
      buffer->size = 0;
      pipe_buffer_array[index] = buffer;

      //FDs 0 to MAX_FDs - 1 are for regular FDs,
      //FDs MAX_FD to MAX_FD + MAX_PIPES * 2 - 1 are for pipe FDs 
      //Each index in pipe_buffer_array map to two file descriptors, read end and write end
      pipe[0] = MAX_FD + index * 2; //read end
      pipe[1] = pipe[0] + 1;  //write end

      buffer->fd_read = pipe[0];
      buffer->fd_write = pipe[1];
    }
    else
    {
      //index is in use, check next one
      index++;
    }    
  }
  return 0;
}

static int 
wait (int pid)
{
  return process_wait (pid);  
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
  lock_acquire(&filesys_lock);
  int i;
  if (!verify_ptr(buffer) || !verify_ptr(buffer + size))
  {
    lock_release(&filesys_lock);
    exit(-1);
  } 
  int bytes_read; 
  if(fd < MAX_FD && fd >= 0)
  {
    //case where FD is for a regular file
    if (fd == 0) 
    {
      for (i = 0; i < size; i++)
      {
        *(char *)(buffer + i) = input_getc();
      }
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
    bytes_read = file_read(file, buffer, size);
  }
  else if((fd < (MAX_FD + MAX_PIPES * 2)) && (fd >= 0)) //each pipe is 2 FDs
  {
    //case where the FD is part of a pipe
    if(fd % 2 == 1)
    {
      //odd pipe FDs are for writing ends of the pipe, can't read from it!
      lock_release(&filesys_lock);
      return -1;
    }
    //find the index the pipe_buffer struct is stored in our pipe_buffer array
    int pipe_array_index = (fd - MAX_FD) / 2;
    struct pipe_buffer *pipe_buffer = pipe_buffer_array[pipe_array_index];
    int read_index = pipe_buffer->start; //next char to be read
    uint32_t buffer_index = 0; //index to write to in return buffer
    int chars_in_pipe_buffer = pipe_buffer->size; //number of chars we can read
    while((buffer_index != size) && (chars_in_pipe_buffer > 0))
    {
      //while there is room to read and space in the buffer to read to:
      ((char *)buffer)[buffer_index] = pipe_buffer->buffer[read_index];      
      read_index = (read_index + 1) % PIPE_BUFFER_SIZE;
      buffer_index++;
      chars_in_pipe_buffer--;
    }
    //update inforamtion in pipe buffer struct
    pipe_buffer->start = read_index;
    pipe_buffer->size = chars_in_pipe_buffer;
    bytes_read = buffer_index;
  }
  else
  {
    //invalid FD
    lock_release(&filesys_lock);
    return -1;
  }
  lock_release(&filesys_lock);
  return bytes_read;
}

static int 
write (int fd, const void *buffer, unsigned size)
{
  lock_acquire(&filesys_lock);
  if (!verify_ptr(buffer) || !verify_ptr(buffer + size))
    exit(-1);

  int bytes_written;
  if(fd >= 0 && fd < MAX_FD)
  {
    //case where we are writing to a normal file
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

    bytes_written = file_write(file, buffer, size);
  }
  else if((fd >= 0) && (fd < (MAX_FD + MAX_PIPES * 2)))
  {
    //case where the FD is part of a pipe
    if(fd % 2 == 0)
    {
      //even pipe FDs are for reading ends of the pipe, you can't write to it!
      lock_release(&filesys_lock);
      return -1;
    }

    int pipe_array_index = (fd - MAX_FD - 1) / 2;
    struct pipe_buffer *pipe_buffer = pipe_buffer_array[pipe_array_index];
    int write_index = pipe_buffer->end; //index to write next char
    uint32_t buffer_index = 0; //index to read next char from
    int chars_in_pipe_buffer = pipe_buffer->size; //how many chars are currently in the pipe_buffer
    while((chars_in_pipe_buffer != PIPE_BUFFER_SIZE) && (buffer_index != size))
    {
      //while there are chars left to write and room in the buffer to write them:
      pipe_buffer->buffer[write_index] = ((char *)buffer)[buffer_index];
      write_index = (write_index + 1) % PIPE_BUFFER_SIZE;
      buffer_index++;
      chars_in_pipe_buffer++;
    }
    //update information in our pipe buffer struct
    pipe_buffer->end = write_index;
    pipe_buffer->size = chars_in_pipe_buffer;
    bytes_written = buffer_index;
  }
  else
  {
    //invalid FD
    lock_release(&filesys_lock);
    return -1;
  }
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
  lock_acquire(&filesys_lock);
  if((fd >= 0) && (fd < MAX_FD))
  {
    //case where FD is for a regular file
    struct file *closed = thread_current()->fds[fd];

    if (closed != NULL)
    {
      thread_current()->fds[fd] = NULL;
      file_close (closed);
    }
  }
  else if((fd >= 0) && (fd < (MAX_FD + MAX_PIPES * 2)))
  {
    //case where FD is for a pipe
    bool read_end = true;
    if(fd % 2 == 1)
    {
      read_end = false;
      //fd (2x) and (2x + 1) both map to the same pipe_array_index.
      //one is read end, other is write end
      fd--;
    }
    int pipe_array_index = (fd - MAX_FD) / 2; 
    struct pipe_buffer *pipe_buffer = pipe_buffer_array[pipe_array_index];
    if(read_end)
    {
      pipe_buffer->fd_read = -1;
    }
    else
    {
      pipe_buffer->fd_write = -1;
    }

    //if both ends are closed, free the memory
    if((pipe_buffer->fd_write == -1) && (pipe_buffer->fd_read == -1))
    {
      free(pipe_buffer);
      pipe_buffer_array[pipe_array_index] = NULL;
    }
  }
  //else invalid FD, nothing to close
  lock_release(&filesys_lock);
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

static mapid_t mmap(int fd, void *addr)
{
//  printf("MMAP CALLED!\n");
  //check for valid fd
  if(fd < 2 || fd >= MAX_FD)
  {
    return -1;
  }

  //check for valid addr
  if(addr == NULL || addr == 0x0)
  {
    return -1;
  }

  //check addr is page aligned
  if(pg_ofs(addr) != 0)
  {
    return -1;
  }
  //fail on
  //page of pages mapped overlaps any existing set of mapped pages, including the stack or pages mapped at executable load time
  struct thread *t = thread_current();
  struct file *file = t->fds[fd];
  if(file == NULL)
  {
    return -1;
  }
  //check valid file length
  off_t length = file_length(file);
  if(length <= 0)
  {
    return -1;
  }
  
  //check if there is enough space to mmap file
  int index = 0;
  while(index < length)
  {
//    printf("while loop...\n");
    //Make sure the page is not already in the suppl page table or pagedir
    bool suppl_pte_exists = vaddr_to_suppl_pte(addr + index);
    bool pagedir_exists = pagedir_get_page(t->pagedir, addr + index);
    if(suppl_pte_exists || pagedir_exists)
    {
      return -1;
    }
    index += PGSIZE;
  }
  lock_acquire(&filesys_lock);
  struct file *mmf_file = file_reopen(file);
  lock_release(&filesys_lock);  

  if(mmf_file == NULL)
  {
    return -1;
  }

  //put file in mm_file struct and add to current threads hash of MMFs
  struct mm_file *mmap_file = malloc(sizeof (struct mm_file));
  if(mmap_file == NULL)
  {
    return -1;
  }

  mapid_t id = t->next_id;
  t->next_id = id + 1;

  mmap_file->mm_id = id;
  mmap_file->file = file;
  mmap_file->start_addr = addr;
//  mmap_file->type = MMF;
  index = 0;
  int page_count = 0;
  int bytes;
  while(length > 0)
  {
//    printf("while loop 2 ...%d\n", length);
    if(length < PGSIZE)
    {
      bytes = length;
    }
    else
    {
      bytes = PGSIZE;
    }
    uint32_t zero_bytes = PGSIZE - bytes;
    bool insert = suppl_pt_insert_mmf(mmf_file, index, addr, bytes, zero_bytes, id);  
    //file
    //file_offset
    //bytes_read
    //bytes_zero
    //vaddr
    //writable

    if(!insert)
    {
      return -1;
    }
    index += PGSIZE;
    length -= PGSIZE;
    addr += PGSIZE;
    page_count++;
  }
//  printf("EXIT WHILE LOOP\n");
  mmap_file->pg_count = page_count;

  hash_insert(&t->mm_files, &mmap_file->elem);
/*
  struct hash_elem *success = hash_insert(&t->mm_files, &mmap_file->elem);
  if(success == NULL)
  {
    printf("noooooo.....\n");
    return -1;
  }
*/
//  printf("~~~~~~~~~~~~~~~~~~~~~~~~~~SUCCESS\n");

  return id;
}

static void munmap(mapid_t mapping)
{
  struct mm_file mm_file_lookup;
  mm_file_lookup.mm_id = mapping;
  struct thread *t = thread_current();
  struct hash_elem *elem = hash_delete(&t->mm_files, &mm_file_lookup.elem);
  if(elem != NULL)
  {
    struct mm_file *mm_file = hash_entry(elem, struct mm_file, elem);
    
    int pg_count = mm_file->pg_count;
    void *start_addr = mm_file->start_addr;
    int count = 0;
    while(pg_count > 0)
    {
      //look up each page in suppl_pt and remove
      struct suppl_pte pte;
      pte.vaddr = start_addr + count * PGSIZE; 
      struct hash_elem *elem = hash_delete(&t->suppl_page_table, &pte.elem);
      if(elem != NULL)
      {
        struct suppl_pte *pte_ptr = hash_entry(elem, struct suppl_pte, elem);
        if(pte_ptr->loaded && pagedir_is_dirty(t->pagedir, pte_ptr->vaddr))
        {
          //write back to disk
          lock_acquire(&filesys_lock);
          file_seek(pte_ptr->file, pte_ptr->file_offset);
          file_write(pte_ptr->file, pte_ptr->vaddr, pte_ptr->bytes_read);
  

          lock_release(&filesys_lock);
        }
      }
      pg_count--;
      count++;
    }
    lock_acquire(&filesys_lock);
    file_close(mm_file); 
    lock_release(&filesys_lock);
  }
}

bool remove (const char *file_name)
{
  if(!verify_ptr(file_name))
  {
    exit(-1);
  }
  lock_acquire(&filesys_lock);
  bool success = filesys_remove(file_name); 
  lock_release(&filesys_lock);
  return success;
}
