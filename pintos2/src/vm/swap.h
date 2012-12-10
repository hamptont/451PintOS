#ifndef VM_SWAP_H
#define VM_SWAP_H

void init_swap_table (void);
size_t mem_to_swap (const void *);
void swap_to_mem (size_t, void *);

#endif
