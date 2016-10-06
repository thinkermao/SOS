#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "types.h"

extern char end[];

// save 64 * PAGE_SIZE for mem bitmap
#define MEM_BITMAP_BASE     (PGROUNDUP((uint32_t)end))
#define MEM_BITMAP_END      ((MEM_BITMAP_BASE) + (PAGE_SIZE << 6))
#define KERNEL_HEAP_START   (MEM_BITMAP_END)

void pmm_init(void);
uint32_t alloc_physic_pages(uint32_t n);
void free_physic_pages(uint32_t pa, uint32_t size);

#endif /* _MEM_LAYOUT_H_ */