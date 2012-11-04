#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define PIPE_BUFFER_SIZE 1024 //max number of chars that can be stored in a pipe buffer
#define MAX_PIPES 128 //max number of pipes that can be open at any given time
#define MAX_FD 128 //max number of regular file descriptors a thread can have open

struct lock filesys_lock;

void syscall_init (void);
void exit (int status);

struct pipe_buffer{
  char buffer[PIPE_BUFFER_SIZE]; //pipe buffer
  int start; //pointer to next char to be read from buffer
  int end; //pointer to index to write next char to buffer
  int fd_read; //FD for read end of pipe
  int fd_write; //FD for write end of pipe
  int size; //number of chars stored in buffer
};

//array to store all the pipes 
struct pipe_buffer *pipe_buffer_array[MAX_PIPES];

#endif /* userprog/syscall.h */
