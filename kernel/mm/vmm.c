#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/memlayout.h>
#include <libs/stdio.h>
#include <libs/debug.h>
#include <libs/string.h>

MemoryLayout * MemoryLayoutCreate(void) 
{
    MemoryLayout *mm = kmalloc(sizeof(MemoryLayout));
    if (mm == NULL)
        return NULL;

    mm->cache = NULL;
    mm->pgdir = NULL;
    list_init(&mm->list);
    return mm;
}

void DestroyMemoryLayout(MemoryLayout *mm)
{
    assert(mm && "nullptr exception");

    struct list_node_t *node = NULL;
    while ((node = list_pop_back(&mm->list)) != list_end()) {
        MemoryArea *ma = GET_MEMORY_AREA_FROM_LIST_NODE(node);
        DestroyMemoryArea(ma);
    }
    if (mm->pgdir != NULL) 
        DestroyPageDirectory(mm->pgdir);
    kfree(mm);
}

MemoryArea * MemoryAreaCreate(uint32_t start, uint32_t end, uint32_t flags)
{
    MemoryArea *ma = kmalloc(sizeof(MemoryArea));
    if (ma == NULL) 
        return NULL;

    ma->start = start;
    ma->end = end;
    ma->flags = flags;
    ma->layout = NULL;
    list_node_init(&ma->node);
    return ma;
}

void DestroyMemoryArea(MemoryArea *ma) 
{
    assert(ma && "nullptr exception");

    if (ma->layout != NULL) {
        uintptr_t start = ma->start;
        size_t size = ma->end - ma->start;
        for (size_t i = 0; i < size; ++i) {
            uintptr_t pa = UnmapUserSpacePage(ma->layout->pgdir, start + i);
            PhysicFreePage(VirtualAddressToPage(P2V(pa)));
        }
    }
    kfree(ma);
}

// (vma->vm_start <= addr <= vma_vm_end)
static bool IsAddressInArea(MemoryArea *ma, uintptr_t address)
{
    assert(ma && "nullptr exception");
    return ma->start <= address && ma->end >= address;
}

MemoryArea * FindMemoryArea(MemoryLayout *mm, uintptr_t address)
{
    assert(mm && "nullptr exception");
    MemoryArea *ma = mm->cache;
    if (ma != NULL && IsAddressInArea(ma, address))
        return ma;
    list_for_each(node, &mm->list) {
        ma = GET_MEMORY_AREA_FROM_LIST_NODE(node);
        if (ma != NULL && IsAddressInArea(ma, address)) {
            mm->cache = ma;
            return ma;
        }
    }
    return NULL;
}

// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void CheckVMOverlap(MemoryArea *prev, MemoryArea *next) 
{
    assert(prev->start < prev->end);
    assert(prev->end <= next->start);
    assert(next->start < next->end);
}

static bool MemoryAreaCmp(struct list_node_t *l, struct list_node_t *r)
{
    assert(l && r && "nullptr exception");
    MemoryArea *left = GET_MEMORY_AREA_FROM_LIST_NODE(l);
    MemoryArea *right = GET_MEMORY_AREA_FROM_LIST_NODE(r);
    return left->start > right->start;
}

/* merge area and return large one. */
static MemoryArea * TryMergeMemoryArea(MemoryArea *prev, MemoryArea *next)
{
    CheckVMOverlap(prev, next);
    if (prev->end == next->start && prev->flags == next->flags) {
        prev->end = next->end;
        list_remove(&next->node);
        next = prev;
    }
    return next;
}

void InsertMemoryArea(MemoryLayout *mm, MemoryArea *ma) 
{
    assert(mm && ma && "nullptr exception");
    assert(ma->start <= ma->end && "logic error");

    list_insert_with_sort(&mm->list, &ma->node, MemoryAreaCmp);
    ma->layout = mm;

    /* check overlap */
    struct list_node_t *prev = list_node_prev(&ma->node);
    if (prev != list_end()) 
        ma = TryMergeMemoryArea(GET_MEMORY_AREA_FROM_LIST_NODE(prev), ma);
    
    struct list_node_t *next = list_node_next(&ma->node);
    if (next != list_end())
        TryMergeMemoryArea(ma, GET_MEMORY_AREA_FROM_LIST_NODE(next));
}

int MemoryMap(MemoryLayout *mm, uintptr_t addr, size_t len, uint32_t flags)
{
    uintptr_t start = PGROUNDDOWN(addr), end = PGROUNDUP(addr + len);

    if (!USER_ACCESS(start, end))
        return -1;

    MemoryArea *ma = FindMemoryArea(mm, start);
    if (ma != NULL && ma->start < end) {    /* FIXME: */
        return -1;
    }
    ma = MemoryAreaCreate(start, end, flags);
    if (ma == NULL) 
        return -1;
    InsertMemoryArea(mm, ma);
    return 0;
}

static void CopyMemory(MemoryLayout *mm, uintptr_t start, size_t size)
{
    assert(mm && mm->pgdir && "please initialize pgdir first");

    // FIXME: page == NULL
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        Page *page = PhysicAllocatePage();
        char *mem = (char*)PageToVirtualAddress(page);
        MapUserSpacePage(mm->pgdir, start + i, V2P(mem), PTE_W);
        /* notice, current pgdir is current Process' pgdir, 
            so target address is mem, source is start + i */
        memmove(mem, (void*)(start + i), PAGE_SIZE);
    }
}

int CopyMemoryMap(MemoryLayout *from, MemoryLayout *to)
{
    assert(from && to && "nullptr exception");
    
    list_for_each(node, &from->list) {
        MemoryArea *ma = GET_MEMORY_AREA_FROM_LIST_NODE(node);
        MemoryArea *nma = MemoryAreaCreate(ma->start, ma->end, ma->flags);
        if (nma == NULL)
            return -1;
        InsertMemoryArea(to, nma);
        CopyMemory(to, ma->start, ma->end - ma->start);
    }
    return 0;
}

void ExitMemoryMap(MemoryLayout *mm)
{
    assert(mm && "nullptr exception");

    DestroyMemoryLayout(mm);
}

void SetupVirtualMemoryManager(void)
{
    printk("++ setup virtual memory manager\n");
}

// Set kernel page directory table.
// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void InitUserVM(MemoryLayout *mm, void *start, size_t size)
{
    assert(mm && start && "nullptr exception");

    mm->pgdir = SetupPageDirectory();
    if (mm->pgdir == 0) 
        panic("cannot initialize user kernle vm.\n");

    if (size >= PAGE_SIZE)
        panic("InitUserVM: more than a page");

    if (MemoryMap(mm, USER_BASE, PAGE_SIZE, 0) != 0) 
        panic("cannot initialize user vm");

    Page *page = PhysicAllocatePage();
    char *mem = (char*)PageToVirtualAddress(page);
    MapUserSpacePage(mm->pgdir, USER_BASE, V2P(mem), PTE_W);
    memmove(mem, start, size);
}