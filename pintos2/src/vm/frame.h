#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct frame 
{
//  void *page;
//  struct thread *thread;
  uint32_t *pte; 
  void *frame;
  tid_t tid;
  struct list_elem frame_elem;
};

struct list frame_list;
struct lock frame_lock;

void frame_init (void);
void *frame_get_page(enum palloc_flags);
void frame_free_page (void *);
static bool add_frame(void *f);
#endif /* vm/frame.h */
