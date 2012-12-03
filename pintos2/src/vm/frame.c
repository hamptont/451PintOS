#include "frame.h"
#include "threads/malloc.h"


static void *frame_replace_page (void);

void frame_init (void)
{
  list_init (&frame_list);
}

struct frame *frame_get_page(enum palloc_flags flags) 
{
  ASSERT (flags & PAL_USER);
  
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
}

void frame_free_page (void *page)
{

}

static void *frame_replace_page (void) 
{

}
