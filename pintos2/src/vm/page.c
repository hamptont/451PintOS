#include "vm/page.h"
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
  
  return pte;

}
