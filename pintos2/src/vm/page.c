#include "vm/page.h"
#include "lib/kernel/hash.h"
#include <stdio.h>
#include <string.h>


/*
 * Looks up a virtural address in the supplemental page table. 
 * Returns the struct suppl_pte for the virtural address
 * Retuns NULL on invalild virtural address
 */
struct suppl_pte sup_pt_lookup(uint32_t *vaddr)
{
  struct suppl_pte pte;
  
//  struct hash suppl_pt = thread_current()->supple_page_table;
  //look up pte in current_thread
//  struct hash_elem *elem = hash_find(suppl_pt,  

  return pte;

}

/*
 * Adds a suplemental page entry to the suppl page table.
 * This function supports adding pages from a file
 */ 
bool suppl_pt_insert_file(void *vaddr, struct file *file, off_t offset, uint32_t bytes_read, bool writable)
{
  struct suppl_pte *pte  = calloc(1, sizeof (struct suppl_pte));
  if(pte == NULL)
  {
    return false;
  }

  pte->vaddr = vaddr;
  pte->type = FILE;
  pte->file = file;
  pte->file_offset = offset;
  pte->bytes_read = bytes_read;
  pte->writable = writable;
 
  struct thread *t = thread_current();
  //can't get this to compile
  //need to add page to thread_current()'s suppl hash table
  /*
  struct hash_elem *success = hash_insert(&t->suppl_page_table, &pte->elem);
  if(success != NULL)
  {
    return false;
  }
  */
  return true;
}

/* 
 * Returns a hash value for suppl_pte p 
 * Functionality required by hash table
 */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct suppl_pte *pte = hash_entry(p_, struct suppl_pte, elem);
  return hash_bytes (&pte->vaddr, sizeof pte->vaddr);
}

/*
 * Returns true if page a precedes page b 
 * Functionality required by hash table
 */

bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct suppl_pte *a = hash_entry(a_, struct suppl_pte, elem);
  const struct suppl_pte *b = hash_entry(b_, struct suppl_pte, elem);
  return a->vaddr < b->vaddr;
}
