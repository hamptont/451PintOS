#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct frame 
{
  uint32_t *pte;
  void *uvaddr;
  void *frame;
  tid_t tid;
  struct list_elem frame_elem;
};

struct list frame_list;
struct lock frame_lock;
struct lock evict_lock;

void frame_init (void);
void *frame_get_page(enum palloc_flags);
void frame_free_page (void *);
void frame_set_user_page (void *, void *, uint32_t *);
void frame_remove_thread (struct thread *);

#endif /* vm/frame.h */
