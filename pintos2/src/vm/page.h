#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include <inttypes.h>
#include <stdbool.h>


enum suppl_pte_type{
  SWAP = 1,
  FILE = 2,
  MMF = 3
};

struct suppl_pte{
  void *vaddr; //user virtural address for page
  enum suppl_pte_type type;  //type of data stored in pte
  struct hash_elem elem; //to look up pte in hash table

  //data about PTE
  struct file *file;
  off_t file_offset;
  uint32_t bytes_read;
  bool writable;
};


bool load_page(struct suppl_pte *pte);
bool load_page_swap(struct suppl_pte *pte);
bool load_page_file(struct suppl_pte *pte);
bool load_page_mmf(struct suppl_pte *pte);
struct suppl_pte *vaddr_to_suppl_pte(struct hash *hash, uint32_t *vaddr);
bool suppl_pt_insert_file(void *vaddr, struct file *file, off_t offset, uint32_t bytes_read, bool writable);
unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
#endif /* vm/page.h */
