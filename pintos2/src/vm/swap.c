#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include "vm/swap.h"

#define BLOCK_SECTORS_PER_PAGE ((size_t) (PGSIZE) / (BLOCK_SECTOR_SIZE))

static struct bitmap *swap_map;
struct block *swap_device;

static block_sector_t num_pages_in_swap (void);

/* Initializes the swap map and the swap device so
 * we can access the swap disk */
void init_swap_table (void)
{
  swap_device = block_get_role (BLOCK_SWAP);

  swap_map = bitmap_create (num_pages_in_swap ());

  bitmap_set_all (swap_map, false);
}

/* Copies a page from memory onto the swap disk */
size_t mem_to_swap (const void *addr)
{
  size_t swap_index = bitmap_scan_and_flip (swap_map, 0, 1, false);

  if (swap_index == BITMAP_ERROR)
    return SIZE_MAX;

  size_t current_block_sector = 0;
  while (current_block_sector < BLOCK_SECTORS_PER_PAGE)
  {
    block_write (swap_device, current_block_sector
        + swap_index * BLOCK_SECTORS_PER_PAGE, 
        addr + current_block_sector * BLOCK_SECTOR_SIZE);
    current_block_sector++;
  }

  return swap_index;
}

/* Copied a page from the swap disk into memory */
void swap_to_mem (size_t swap_index, void *addr)
{
  size_t current_block_sector = 0;
  while (current_block_sector < BLOCK_SECTORS_PER_PAGE)
  {
    block_read (swap_device, current_block_sector
        + swap_index * BLOCK_SECTORS_PER_PAGE, 
        addr + current_block_sector * BLOCK_SECTOR_SIZE);
    current_block_sector++;
  }

  bitmap_flip (swap_map, swap_index);
}

/* Calculates the number of pages that the swap
 * disk can hold */
static block_sector_t num_pages_in_swap (void)
{
  return block_size (swap_device) / BLOCK_SECTORS_PER_PAGE;
}
