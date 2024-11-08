#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;
}
#endif

void freePage(ptr_t baseAddr)
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
    PTE *lv3_pgdir = (PTE *) pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
    if (lv3_pgdir[vpn2] == 0) {     // alloc a new second-level page directory
        PTE *lv2_pgdir = allocPage(1);
        set_pfn(&lv3_pgdir[vpn2], kva2pa(lv2_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv3_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(lv2_pgdir);   // clear second-level pgdir page
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {     // alloc a new first_level page directory
        PTE *lv1_pgdir = allocPage(1);
        set_pfn(&lv2_pgdir[vpn1], kva2pa(lv1_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv2_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(lv1_pgdir);   // clear second-level pgdir page
    }
    PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
    uint32_t upa = kva2pa(allocPage(1));
    set_pfn(&lv1_pgdir[vpn0], upa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &lv1_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_DIRTY);
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}