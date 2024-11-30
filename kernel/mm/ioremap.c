#include <os/ioremap.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

int iomap_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
    PTE *lv3_pgdir = (PTE *) pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
    if (lv3_pgdir[vpn2] == 0) {     // alloc a new second-level page directory
        PTE *lv2_pgdir = (PTE *) alloc_kernel_page();
        set_pfn(&lv3_pgdir[vpn2], (uint64_t) kva2pa(lv2_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv3_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(lv2_pgdir);   // clear second-level pgdir page
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {     // alloc a new first_level page directory
        PTE *lv1_pgdir = (PTE *) alloc_kernel_page();
        set_pfn(&lv2_pgdir[vpn1], kva2pa(lv1_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv2_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(lv1_pgdir);   // clear second-level pgdir page
    }
    PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
    // uintptr_t upa = kva2pa(kva);
    set_pfn(&lv1_pgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &lv1_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_DIRTY);
    local_flush_tlb_all();
    return 1;
}


void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    uintptr_t pa_start = ROUNDDOWN(phys_addr, PAGE_SIZE);
    uintptr_t pa_end = ROUND(phys_addr + size, PAGE_SIZE);
    size = pa_end - pa_start;
    if (io_base + size <= IO_ADDR_END) {
        uintptr_t io_start = io_base;
        for (uintptr_t pa = pa_start;pa < pa_end;pa += PAGE_SIZE) {
            iomap_page(io_base, pa, current_running->pgdir);
            io_base += PAGE_SIZE;
        }
        return io_start;
    } else {
        return NULL;
    }
}

void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
    PTE *pte = kva2pte(io_addr, current_running->pgdir);
    clear_attribute(pte, _PAGE_PRESENT);
}
