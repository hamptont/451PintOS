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

/* Returns a hash value for suppl_pte p */
unsigned page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct suppl_pte *pte = hash_entry(p_, struct suppl_pte, elem);
  return hash_bytes (&pte->vaddr, sizeof pte->vaddr);
}

/* Returns true if page a precedes page b */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct suppl_pte *a = hash_entry(a_, struct suppl_pte, elem);
  const struct suppl_pte *b = hash_entry(b_, struct suppl_pte, elem);
  return a->vaddr < b->vaddr;
}
