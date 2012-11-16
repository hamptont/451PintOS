/* 
 * Gracious thanks to
 * Suman Jandhyala, Riley Adams, and Alan Ridwan
 * from whose fork implementation these methods were inspired.
 *
 * forkutils
 *
 * A set of functions to be used to write an implementation of fork in Pintos.
 * These aren't quite plug-and-play - read the comments for where you need
 * to wrangle them into your implementation. Main roadblocks:
 *
 * -init_thread and allocate_tid are private (aka static, in C) to thread.c
 * You need to make a way to call them, or change this to call non-static
 * helper functions that call the static functions.
 *
 * -These functions also don't guarantee that all your child/waiting/FD
 * related data structures are copied. Obviously that's dependent on your
 * implementation.
 *
 * -Might have to think about writability of the new process's executable.
 *
 * You might want to add this file (and a corresponding .h file) to the 
 * project, or you might just want to copy these functions (and any missing
 * #includes) into your existing work. Whichever works better for you.
 * You might also want to change these functions, since they don't compile
 * as it stands.
 *
 * Your fork might look like:
 *
 * //pseudocode
 * syscall_fork(struct intr_frame *f) {
 *  child = create_child_thread()
 *  setup_thread_to_return_from_fork(child, f)
 *  child->pagedir = pagedir_duplicate(thread_current()->pagedir) 
 *  thread_unblock(child_thread)
 *  return child_thread->tid
 * }
 *
 * With stuff added in to account for your file descriptors, children,
 * etc.
 * */

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "threads/switch.h"
#include <stdio.h>
#include <string.h>

#include "userprog/forkutils.h"

uint32_t* pagedir_duplicate (uint32_t*);
struct thread * create_child_thread (void);
void jump_to_intr_exit(struct intr_frame);
void setup_thread_to_return_from_fork(struct thread *, struct intr_frame *);

/*
 * pagedir_duplicate takes a source page directory and returns a pointer
 * to a new page directory which is a copy of the source directory.
 *
 * Each user virtual address in the source can be found in the new
 * directory, and points to a new page which contains a copy of the
 * data in the frmae mapped by that address in the source.
 * 
 * Returns NULL on failure (e.g. failure of pagedir_create or
 * palloc_get_page) and a valid pointer to a new, duplicate
 * pagedir on success.
 */
uint32_t*
pagedir_duplicate (uint32_t* src_pd) {
  /* Allocate a new page directory */
  uint32_t *copy_pd = pagedir_create();

  /* then iterate through the old one, copying as we go */
  uint32_t *pd = src_pd; 
  uint32_t *pde;
  for (pde = pd; pde < pd + pd_no(PHYS_BASE); ++pde) {
    if (*pde & PTE_P) {
      uint32_t *pt = pde_get_pt(*pde);
      uint32_t *pte;
      for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++) {
        if (*pte & PTE_P) {
          uint32_t* kpage = palloc_get_page (PAL_USER | PAL_ZERO);
          memcpy(kpage, pte_get_page(*pte), PGSIZE);
          uint32_t vdr = ((pde - pd) << PDSHIFT) | ((pte - pt) << PGBITS);
          pagedir_set_page(copy_pd, (void*)vdr, kpage, *pte & PTE_W);
        }
      }
    }
  } 
  return copy_pd;
} 

/*
 * Creates a new struct thread and initializes its fields.
 * The new thread is also added to the all threads list.
 * Sets name to current thread's name, priority to current thread's priority, etc.
 * You may need to also initialize any fields you have added to struct
 * thread here, such as child lists, exit/wait related fields, etc.
 *
 */

struct thread *
create_child_thread () {
  /* Allocate thread. */
  struct thread* new_thread = palloc_get_page (PAL_ZERO);
  if (new_thread == NULL)
    return NULL;
  
  /* Initialize Thread 
   *
   * init_thread in thread.c is static, which means this won't work.
   * You can either redeclare it in thread.h and make it not static
   * or write a helper function that lets you call init_thread from 
   * outside of thread.c
   *
   * The point is you need to do what init_thread does (set the thread
   * to be blocked, set THREAD_MAGIC properly, etc.)
   *
   * Also, make sure you're setting up the child/waiting structures for
   * the child properly. If you don't do that in init_thread, you should
   * do it here or in your fork implementation so that, for example,
   * your children can have children.
   *
   * (static functions in C can't be called from outside their source file)
   *
   */
  global_init_thread(new_thread, thread_current()->name, thread_get_priority());

  /* 
   * Init thread doesn't allocate a tid 
   *
   * Again, this doesn't work - allocate_tid() is static. You can write an
   * access routine or make it not static, as you wish.
   */
  new_thread->tid = global_allocate_tid();

  return new_thread; 

}

/*
 * Simulates a return from an interrupt by setting the stack pointer to point
 * to the passed interrupt frame and jumping to intr_exit.
 * */
void
jump_to_intr_exit(struct intr_frame frame) {
  intr_enable ();
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&frame) : "memory");
  NOT_REACHED();
}

/* 
 * This function sets up the stack of the child thread so that it will end up
 * executing jump_to_intr_exit.
 *
 * This is basically what happens in thread_create, except that the last
 * function here is jump_to_intr_exit() instead of kernel_thread().
 *
 * After this function, the kernel stack of the new thread looks like:
 *
 * ------------------------ 4k
 * | jump_to_intr_exit()  |
 * | switch_entry()       | stack grows downward
 * | switch_threads()     |
 * |                      |
 * |        ...           |
 *
 * Thus it appears that the thread was running switch_threads(), which is
 * what the scheduler expects when it calls switch_threads to change to
 * this thread the first time. Switch_threads will return to switch_entry
 * which will return to jump_to_intr_exit() which will jump to intr_exit
 * which will restore registers and jump back into user space.
 *
 * Phew!
 *
 * */
void
setup_thread_to_return_from_fork(struct thread *new_thread, struct intr_frame *f) {
  /* setup stack on the new thread such that we'll jump into the right place */
  struct {
    void *eip;             /* Return address. */
    struct intr_frame if_; /* Interrupt frame we'll jump into */
  } *fp;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;

  /* Stack frame for jump_to_intr_exit() */
  new_thread->stack -= sizeof *fp;
  fp = new_thread->stack;
  fp->eip = NULL;
  fp->if_ = *f;
  fp->if_.eax = 0; // set return value to 0, so child gets a 0 out of fork()

  /* Stack frame for switch_entry(). */
  new_thread->stack -= sizeof *ef;
  ef = (struct switch_entry_frame*)new_thread->stack;
  ef->eip = (void (*) (void)) jump_to_intr_exit;

  /* Stack frame for switch_threads(). */
  new_thread->stack -= sizeof *sf;
  sf = (struct switch_threads_frame*)new_thread->stack;
  sf->eip = switch_entry;
  sf->ebp = 0;
}


