#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct frame 
{
  void *page;
  struct thread *thread;

  struct list_elem frame_elem;
};

struct list frame_list;

void frame_init (void);
struct frame *frame_get_page(enum palloc_flags);
void frame_free_page (void *);

#endif /* vm/frame.h */
