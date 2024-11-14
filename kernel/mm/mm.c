#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

// Each bit represents the state of a page. When initialized, 
// all bits are set to 0, indicating that all pages are unallocated.
char bitmap[FREE_MEM_PAGE_NUM / 8] = { 0 }; // Each char has 8 bits to represent 8 pages
void init_bitmap() {
    // 0xffffffc052000000 - 0xffffffc052004000 are used as stack, first 4 page
    bitmap[0] |= 0x0f;    // 0b1111
}
int find_free_pages(int numPage) {
    int consecutive_count = 0;
    int start_page = -1;

    // Traverse each byte in the bitmap
    for (int byte_idx = 0; byte_idx < FREE_MEM_PAGE_NUM / 8; byte_idx++) {
        if (bitmap[byte_idx] != 0xFF) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                int page_idx = byte_idx * 8 + bit;

                if (!(bitmap[byte_idx] & (1 << bit))) {  // Check if the page is free
                    if (consecutive_count == 0) {
                        start_page = page_idx;  // Record the starting position of the free pages
                    }
                    consecutive_count++;

                    // If we have found enough consecutive free pages
                    if (consecutive_count == numPage) {
                        return start_page;
                    }
                } else {
                    // Reset counters if a used page is encountered
                    consecutive_count = 0;
                    start_page = -1;
                }
            }
        } else {
            // Reset counters if the entire byte is occupied
            consecutive_count = 0;
            start_page = -1;
        }
    }
    return -1;  // Not enough consecutive free pages found
}
void mark_page_allocated(int start_page, int numPage) {
    for (int i = 0; i < numPage; i++) {
        start_page = start_page + i;
        bitmap[start_page / 8] |= (1 << (start_page % 8));  // Set the corresponding bit to 1
    }

}

void unmark_page_free(int start_page, int page_num) {
    for (int i = 0; i < page_num; i++) {
        int page_idx = start_page + i;
        bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));  // Clear the corresponding bit to 0
    }
}

ptr_t allocPage(int numPage, pcb_t *pcb)
{
    int start_page = find_free_pages(numPage);
    if (start_page == -1) {
        return -1;      // fail to alloc
    }
    mark_page_allocated(start_page, numPage);
    /* record the alloced page in pcb */
    // pcb->page_occupied[pcb->page_occupied_pointer].numPage = numPage;
    // pcb->page_occupied[pcb->page_occupied_pointer].start_page = start_page;
    // pcb->page_occupied_pointer++;
    ptr_t retval = INIT_KERNEL_STACK + PAGE_SIZE * start_page;
    return INIT_KERNEL_STACK + PAGE_SIZE * start_page;
}
void release_process_page(pcb_t *pcb) {
    // for (;pcb->page_occupied_pointer > 0;pcb->page_occupied_pointer--) {
    //     int i = pcb->page_occupied_pointer;
    //     unmark_page_free(pcb->page_occupied[i].start_page, pcb->page_occupied[i].numPage);
    // }
}

/* This is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // share_pgtable:
    memcpy((uint8_t *) dest_pgdir, (uint8_t *) src_pgdir, PAGE_SIZE); // copy kernel pgdir
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb)
{
    PTE *lv3_pgdir = (PTE *) pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
    uint64_t offset = va & 0xFFF;   // first 12 bit
    if (lv3_pgdir[vpn2] == 0) {     // alloc a new second-level page directory
        PTE *lv2_pgdir = allocPage(1, pcb);
        set_pfn(&lv3_pgdir[vpn2], kva2pa(lv2_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv3_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(lv2_pgdir);   // clear second-level pgdir page
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {     // alloc a new first_level page directory
        PTE *lv1_pgdir = allocPage(1, pcb);
        set_pfn(&lv2_pgdir[vpn1], kva2pa(lv1_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv2_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(lv1_pgdir);   // clear second-level pgdir page
    }
    PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
    uint64_t kva = allocPage(1, pcb);
    uint64_t upa = kva2pa(kva);
    set_pfn(&lv1_pgdir[vpn0], upa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &lv1_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_DIRTY);
    return kva + offset;
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}




//void *kmalloc(size_t size, pcb_t *pcb)
// {
//     // TODO [P4-task1] (design you 'kmalloc' here if you need):
//     size = ROUND(size, 8);   // aligned to 8 B

//     static uintptr_t kmalloc_buffer_begin = 0;
//     static uintptr_t kmalloc_buffer_end = 0;

//     if (kmalloc_buffer_end - kmalloc_buffer_begin < size) {
//         kmalloc_buffer_begin = allocPage(NBYTES2PAGE(size), pcb);
//         kmalloc_buffer_end = kmalloc_buffer_begin + NBYTES2PAGE(size) * PAGE_SIZE;
//     }
//     kmalloc_buffer_begin += size;
//     return kmalloc_buffer_begin - size;
// }