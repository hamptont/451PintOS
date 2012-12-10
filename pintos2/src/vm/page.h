#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/off_t.h"
#include "lib/kernel/hash.h"
#include <inttypes.h>
#include <stdbool.h>


enum suppl_pte_type{
  SWAP = 001,
  FILE = 002,
  MMF = 004
};

struct suppl_pte{
  uint8_t *vaddr; //user virtural address for page
  enum suppl_pte_type type;  //type of data stored in pte
  struct hash_elem elem; //to look up pte in hash table

  //data about PTE
  struct file *file;
  off_t file_offset;
  uint32_t bytes_read;
  uint32_t bytes_zero;
  size_t swap_index;
  bool writable;
  bool loaded;
};

bool load_page(struct suppl_pte *pte);
bool load_page_swap(struct suppl_pte *pte);
bool load_page_file(struct suppl_pte *pte);
bool load_page_mmf(struct suppl_pte *pte);
struct suppl_pte *vaddr_to_suppl_pte(uint32_t *vaddr);
bool insert_suppl_pte(struct hash *, struct suppl_pte *pte);
//bool suppl_pt_insert_file(void *vaddr, struct file *file, off_t offset, size_t bytes_read, size_t bytes_zero, bool writable);
bool suppl_pt_insert_file(uint8_t *vaddr, struct file *file, off_t offset, uint32_t bytes_read, uint32_t bytes_zero, bool writable);
unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
#endif /* vm/page.h */
