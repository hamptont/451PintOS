#include "frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"


static void *frame_replace_page (void);

void frame_init (void)
{
  list_init (&frame_list);
  lock_init (&frame_lock);
}

//allocate page from user_pool
//add to frame table
void *frame_get_page(enum palloc_flags flags) 
{
  ASSERT (flags & PAL_USER);
  void *frame = NULL;
  //get a page from user pool
  if(flags & PAL_USER)
  {
    if(flags & PAL_USER)
    {
      frame = palloc_get_page(PAL_USER | PAL_ZERO);
    }
    else
    {
      frame = palloc_get_page(PAL_USER);
    }
  }

  //on success, add frame to list
  if(frame != NULL)
  {
    add_frame(frame);
  }
  else
  {
    //evict frame
  }
  return frame;

/* 
  struct frame *frame;
  void *page = palloc_get_page(flags);
  if (page == NULL) 
  {
    page = frame_replace_page();
  } 

  frame = malloc (sizeof (struct frame));
  frame->page = page;
  frame->thread = thread_current();

  list_push_back (&frame_list, &frame->frame_elem);
    
  return frame;
*/
}

void frame_free_page (void *page)
{
  struct list_elem *e;
  struct frame *frame;
  for (e = list_begin (&frame_list); e != list_end (&frame_list);
       e = list_next (e))
    {
      frame = list_entry (e, struct frame, frame_elem);
/*
      if (frame->page == page && frame->tid == thread_current()->tid) {
        list_remove (&frame->frame_elem);
      }
*/
      free (frame);
    }

  palloc_free_page(page);
}

//add frame to frame table
static bool add_frame(void *f)
{
  struct frame *frame = malloc(sizeof(struct frame));
  if(frame == NULL)
  {
    return false;
  }
  struct thread *t = thread_current();
  frame->tid = t->tid;
  frame->frame = f;

  lock_acquire(&frame_lock);
  list_push_back(&frame_list, &frame->frame_elem);
  lock_release(&frame_lock);
  return true;
}

static void *frame_replace_page (void) 
{

}
