#include "vm/page.h"
#include "lib/kernel/hash.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include <stdio.h>
#include <string.h>

/*
 * Looks up a virtural address in the supplemental page table. 
 * Returns the struct suppl_pte for the virtural address
 * Retuns NULL if it does not exist in the hash table
 */
struct suppl_pte *vaddr_to_suppl_pte(uint32_t *vaddr)
{
  struct hash suppl_page_table = thread_current()->suppl_page_table;
  struct suppl_pte pte;
  pte.vaddr = vaddr;
  
  struct hash_elem *hash_elem = hash_find(&suppl_page_table, &(pte.elem));
  
  if(hash_elem != NULL)
  {
    return hash_entry(hash_elem, struct suppl_pte, elem);
  }
  return NULL;
}

/* Load Page */
bool load_page(struct suppl_pte *pte)
{
  //find type of file
  enum suppl_pte_type type = pte->type;
  if(type & SWAP)
  {
    return load_page_swap(pte);  
  }
  else if(type == FILE)
  {
    return load_page_file(pte);
  }
  else if(type == MMF)
  {
/*
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
*/
    return load_page_mmf(pte);
  }
  //should not be reached
  return false;
}

bool load_page_swap(struct suppl_pte *spte)
{
  uint8_t *kpage = frame_get_page (PAL_USER);

  if (kpage == NULL)
    return false;

  if (!pagedir_set_page (thread_current ()->pagedir, spte->vaddr, 
        kpage, spte->writable))
  {
    frame_free_page (kpage);
    return false;
  }

  swap_to_mem (spte->swap_index, spte->vaddr);
  if (spte->type == SWAP)
  {
    hash_delete (&thread_current()->suppl_page_table, &spte->elem);
  }
  if (spte->type == (FILE | SWAP)) 
  {
    spte->type = FILE;
    spte->loaded = true;
  }

  return true;
}

bool load_page_file(struct suppl_pte *pte)
{

  struct thread *t = thread_current();
 
  file_seek(pte->file, pte->file_offset); 

  uint32_t bytes_read = pte->bytes_read;
  uint32_t bytes_zero = pte->bytes_zero;
  
  //load the page with info from suppl_pte
  
  //get page of memory
  uint8_t *kpage = frame_get_page (PAL_USER); 
  if(kpage == NULL)
  {
    return false;
  }  
  
  //load page
  off_t b_read = file_read(pte->file, kpage, pte->bytes_read);
  off_t expected_b_read = pte->bytes_read;
  off_t of = file_tell(pte->file);
  struct file *f = pte->file;
  
  if(b_read != expected_b_read)
  { 
    frame_free_page(kpage);
    return false;
  }
  memset(kpage + pte->bytes_read, 0, pte->bytes_zero);

  bool install_page = (pagedir_get_page(t->pagedir, pte->vaddr) == NULL)
		&& (pagedir_set_page (t->pagedir, pte->vaddr, kpage, pte->writable));

  //add page to process's address space
  if(!install_page)
  {
    frame_free_page(kpage);
    return false;
  }  
  pte->loaded = true;
  return true;
}

void write_back_mmf (struct suppl_pte *spte)
{
  if (spte->type == MMF)
  {
    file_seek (spte->file, spte->file_offset);
    file_write (spte->file, spte->vaddr, spte->bytes_read);
  }
}

bool load_page_mmf(struct suppl_pte *pte)
{
  //seek to correct possition in file
  file_seek(pte->file, pte->file_offset);

  //get page from memory
  uint8_t *kpage = frame_get_page(PAL_USER); //??
  if(kpage == NULL)
  {
    return false;
  } 

  //load page
  off_t b_read = file_read(pte->file, kpage, pte->bytes_read);
  off_t expected_b_read = pte->bytes_read;
//  off_t of = file_tell(pte->file);
//  struct file *f = pte->file;
  
  if(b_read != expected_b_read)
  { 
    frame_free_page(kpage);
    return false;
  }
  memset(kpage + pte->bytes_read, 0, pte->bytes_zero);

  struct thread *t = thread_current();

  //load page to current threads address space
  bool install_page = pagedir_set_page(t->pagedir, pte->vaddr, kpage, true);
  if(!install_page)
  {
    frame_free_page(kpage);
    return false;
  }  
  pte->loaded = true;
//  printf("DONEEEEEEEEEEEEEEE!!!!!!!!!!!!!!!!!!\n"); 
  return true;
}

/*
 * Insert a suppl_pte into the hash table
 * Returns true on success, false on failure
 */
bool insert_suppl_pte(struct hash *pt, struct suppl_pte *pte)
{
  if (pte == NULL)
    return false;

  printf ("Inserted frame: %x\n", pte->vaddr);
  struct hash_elem *result = hash_insert (pt, &(pte->elem));
  if (result != NULL)
    return false;

  return true;
}

/*
 * Adds a suplemental page entry to the suppl page table.
 * This function supports adding pages from a file.
 * Returns true on success, false on failure
 */ 
//bool suppl_pt_insert_file(void *vaddr, struct file *file, off_t offset, size_t bytes_read, size_t bytes_zero, bool writable)
bool suppl_pt_insert_file(uint8_t *vaddr, struct file *file, off_t offset, uint32_t bytes_read, uint32_t bytes_zero, bool writable)
{
  struct suppl_pte *pte  = malloc(sizeof (struct suppl_pte));
  if(pte == NULL)
  {
    return false;
  }

  //store info about the file in the suppl page table
  pte->vaddr = vaddr;
  pte->type = FILE;
  pte->file = file;
  pte->file_offset = offset;
  pte->bytes_read = bytes_read;
  pte->bytes_zero = bytes_zero;
  pte->writable = writable;
  pte->loaded = false;

  printf ("Inserted file: %x\n", pte->vaddr);

  //add page to thread_current()'s suppl hash table
  struct thread *t = thread_current();
  struct hash suppl_page_table = t->suppl_page_table;
  struct hash_elem *success = hash_insert(&suppl_page_table, &pte->elem);
  if(success != NULL)
  {
    return false;
  }
  return true;
}

bool suppl_pt_insert_mmf(struct file *f, off_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, int id)
{
  struct suppl_pte *pte  = malloc(sizeof (struct suppl_pte));
  if(pte == NULL)
  {
    return false;
  }

  //store info about the file in the suppl page table
  pte->vaddr = upage;
  pte->type = MMF;
  pte->file = f;
  pte->file_offset = offset;
  pte->bytes_read = read_bytes;
  pte->bytes_zero = zero_bytes;
  pte->loaded = false;
//  pte->mm_id = id;
  pte->writable = true; //????
  pte->type = MMF;
/*
  struct mm_file e;
  e.mm_id = id;

  struct hash_elem *hash_elem =  hash_find(&(thread_current()->mm_files), &(e.elem));
  if(hash_elem == NULL)
  {
    return -1;
  }
  struct mm_file *mm_file = hash_entry(hash_elem, struct mm_file, elem);
  pte->mm_file = *mm_file;
*/
  //add page to thread_current()'s suppl hash table
  struct thread *t = thread_current();
  struct hash suppl_page_table = t->suppl_page_table;
  struct hash_elem *success = hash_insert(&suppl_page_table, &pte->elem);
  if(success != NULL)
  {
    return false;
  }
  return true;
}
/***********************************/
// functions for the hash table    //
/**********************************/

/* 
 * Functional required by hash table
 * Returns a hash value for suppl_pte p 
 */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct suppl_pte *pte = hash_entry(p_, struct suppl_pte, elem);
  return hash_bytes (&pte->vaddr, sizeof pte->vaddr);
}

/*
 * Functional required by hash table
 * Returns true if page a precedes page b 
 */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct suppl_pte *a = hash_entry(a_, struct suppl_pte, elem);
  const struct suppl_pte *b = hash_entry(b_, struct suppl_pte, elem);
  return a->vaddr < b->vaddr;
}

unsigned mmf_hash(const struct hash_elem *p_, void *aux)
{
  const struct mm_file *p = hash_entry(p_, struct mm_file, elem);
  return hash_bytes(&p->mm_id, sizeof(p->mm_id));
}

bool mmf_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
  const struct mm_file *a = hash_entry(a_, struct mm_file, elem);
  const struct mm_file *b = hash_entry(b_, struct mm_file, elem);
  return a->mm_id < b->mm_id;
}
