#include "frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include <string.h>
#include <stdio.h>

static void *frame_replace_page (void);
static struct lock evict_lock;
static struct list_elem *hand;

static bool add_frame (void *);
static void *frame_replace_page (void);
static struct frame *select_evictee (void);
static bool save_frame (struct frame *);
static struct frame *get_frame (void *);

void frame_init (void)
{
  list_init (&frame_list);
  lock_init (&frame_lock);
  lock_init (&evict_lock);
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
    frame = frame_replace_page ();
  }
  return frame;
}

void frame_free_page (void *page)
{
  struct list_elem *e;
  struct frame *frame;
  for (e = list_begin (&frame_list); e != list_end (&frame_list);
       e = list_next (e))
    {
      frame = list_entry (e, struct frame, frame_elem);
      
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

static void *frame_replace_page () 
{
  struct frame *evict_frame;
  struct thread *t = thread_current ();

  lock_acquire (&evict_lock);

  evict_frame = select_evictee ();
  printf ("Evicted frame: %x\n", evict_frame->uvaddr);
  if (evict_frame == NULL)
    printf ("CAN'T SELECT FRAME TO EVICT");

  if (!save_frame (evict_frame))
    printf ("CAN'T SAVE EVICTED FRAME");

  evict_frame->tid = t->tid;
  evict_frame->pte = NULL;
  
  lock_release (&evict_lock);
  return evict_frame;
}

static struct frame *select_evictee (void)
{
  struct thread *t;
  struct list_elem *e;
  struct frame *current_frame;

  bool selected = false;

  if (hand == NULL)
  {
    hand = list_head (&frame_list);
    hand = list_next (hand);
  }
  while (!selected)
  {
    e = hand;
    while (e != list_tail (&frame_list))
    {
      current_frame = list_entry (e, struct frame, frame_elem);
      t = thread_from_tid (current_frame->tid);
      bool dirty = pagedir_is_accessed (t->pagedir, current_frame->uvaddr);
      if (dirty)
      {
        pagedir_set_accessed (t->pagedir, current_frame->uvaddr, false);
      } 
      else 
        break;
      e = list_next (e);
    }
    if (current_frame != NULL)
      selected = true;
    hand = list_head (&frame_list);
    hand = list_next (hand);
  }
  return current_frame;
}

static bool save_frame (struct frame *evict_frame)
{
  struct thread *t;
  struct suppl_pte *spte;
  
  t = thread_from_tid (evict_frame->tid);
  spte = vaddr_to_suppl_pte (evict_frame->uvaddr);

  if (spte == NULL)
  {
    spte = calloc (1, sizeof *spte);
    spte->vaddr = evict_frame->uvaddr;
    spte->type = SWAP;
    if (!insert_suppl_pte (&(t->suppl_page_table), spte))
      return false;
  }

  size_t swap_index;

  if (pagedir_is_dirty (t->pagedir, spte->vaddr) && (spte->type == MMF))
  {
    write_back_mmf (spte);
  }
  else if (pagedir_is_dirty (t->pagedir, spte->vaddr) || (spte->type != FILE))
  {
    swap_index = mem_to_swap (spte->vaddr);
    if (swap_index == SIZE_MAX)
      return false;

    spte->type |= SWAP;
  }

  memset (evict_frame->frame, 0, PGSIZE);

  spte->swap_index = swap_index;
  spte->writable = *(evict_frame->pte) & PTE_W;

  spte->loaded = false;

  pagedir_clear_page (t->pagedir, spte->vaddr);

  return true;
}

void frame_set_user_page (void *frame, void *upage, uint32_t *pte)
{
  struct frame *temp_frame;
  temp_frame = get_frame (frame);
  if (temp_frame != NULL)
  {
    temp_frame->uvaddr = upage;
    temp_frame->pte = pte;
  }
}

static struct frame *get_frame (void *frame)
{
  struct frame *ret_frame;
  struct list_elem *e;

  lock_acquire (&frame_lock);
  e = list_head (&frame_list);
  while ((e = list_next (e)) != list_tail (&frame_list))
  {
    ret_frame = list_entry (e, struct frame, frame_elem);
    if (ret_frame->frame == frame)
      break;
    ret_frame = NULL;
  }
  lock_release (&frame_lock);
  return ret_frame;
}


void frame_remove_thread (struct thread *t)
{
  lock_acquire (&frame_lock);
  struct list_elem *e;
  struct frame *frame;
  for (e = list_begin (&frame_list); e != list_end (&frame_list);
       e = list_next (e))
    {
      frame = list_entry (e, struct frame, frame_elem);
      
      if (frame->tid == t->tid) {
        list_remove (&frame->frame_elem);
      }

      palloc_free_page(frame->uvaddr);
      free (frame);
    }

  lock_release (&frame_lock);
}
