#include <os/mm.h>
#include <os/irq.h>
#include <assert.h>

/* ---------------------------------------------BIT MAP------------------------------------------------------------ */
#define NPAGES2BITMAPCH(npages) (((npages) / 8) + ((npages) % 8 != 0))
// Each bit represents the state of a page. When initialized, 
// all bits are set to 0, indicating that all pages are unallocated.
volatile char user_bitmap[NPAGES2BITMAPCH(FREE_USER_PAGE_NUM)] = { 0 }; // Each char has 8 bits to represent 8 pages
volatile char kernel_bitmap[NPAGES2BITMAPCH(FREE_KERNEL_PAGE_NUM)] = { 0 };
void init_bitmap() {
    // 0xffffffc052000000 - 0xffffffc052004000 are used as stack, first 4 page
    kernel_bitmap[0] |= 0x0f;    // 0b1111
}
void init_mem_manager() {
    // sd buffer addr
    sd_sector_end = (uint64_t) * (short *) nsectors_image_loc;
    // PageNode list init
    pn_list = (PageNode_t *) PN_LIST_BASE;
    for (int i = 0;i < PageNode_MAXNUM;i++) {
        pn_list[i].status = PN_INACTIVE;
    }
    init_list_head(&kernel_page_head);
    init_list_head(&user_page_mem_head);
    init_list_head(&user_page_sd_head);

    init_bitmap();
}
// return the page_idx in the bitmap
uint64_t find_free_pages(char bitmap[], int page_num) {
    // Traverse each byte in the bitmap
    for (uint64_t byte_idx = 0; byte_idx < NPAGES2BITMAPCH(page_num); byte_idx++) {
        if (bitmap[byte_idx] != 0xFF) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                uint64_t page_idx = byte_idx * 8 + bit;
                if (page_idx >= page_num) {
                    return -1;
                }

                if (!(bitmap[byte_idx] & (1 << bit))) {  // Check if the page is free
                    return page_idx;
                }
            }
        }
    }
    return -1;  // Not enough consecutive free pages found
}

void mark_page_allocated(uint64_t page_idx, char bitmap[]) {
    bitmap[page_idx / 8] |= (1 << (page_idx % 8));  // Set the corresponding bit to 1
}

void unmark_page_free(uint64_t page_idx, char bitmap[]) {
    bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));  // Clear the corresponding bit to 0
}


/* -----------------------------------------------------------Alloc Page-------------------------------------------------------------------- */
ptr_t alloc_kernel_page()
{
    uint64_t page_idx = find_free_pages(kernel_bitmap, FREE_KERNEL_PAGE_NUM);
    if (page_idx == -1) {
        return swap_page();      // fail to alloc
    }
    mark_page_allocated(page_idx, kernel_bitmap);

    // return kva
    return INIT_KERNEL_STACK + PAGE_SIZE * page_idx;
}

ptr_t alloc_user_page() {
    uint64_t page_idx = find_free_pages(user_bitmap, FREE_USER_PAGE_NUM);
    if (page_idx == -1) {
        return swap_page();      // fail to alloc
    }
    mark_page_allocated(page_idx, user_bitmap);

    // return kva
    return USER_MEM_BASE + PAGE_SIZE * page_idx;
}
/* This is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // share_pgtable:
    memcpy((uint8_t *) dest_pgdir, (uint8_t *) src_pgdir, PAGE_SIZE); // copy kernel pgdir
}

// ------------------------------------------------------------recycle page ---------------------------------------------------
static inline void recycle_node_from_list(pcb_t *pcb, PageNode_t *head, int is_in_mem) {
    PageNode_t *p = head->next;
    while (p != head) {
        PageNode_t *pnext = p->next;
        if (p->pid == pcb->pid) {
            // recycle PageNode
            p->status = PN_INACTIVE;
            // recycle MemPage
            if (is_in_mem) {
                if (p->uva != p->addr.kva) { // user page
                    uint64_t page_idx = (p->addr.kva - USER_MEM_BASE) / PAGE_SIZE;
                    unmark_page_free(page_idx, user_bitmap);
                } else {
                    uint64_t page_idx = (p->addr.kva - INIT_KERNEL_STACK) / PAGE_SIZE;
                    unmark_page_free(page_idx, kernel_bitmap);
                }
            }
            // remove from the list
            delete_list_node(p);
        }
        p = pnext;
    }
}
void release_process_page(pcb_t *pcb) {
    recycle_node_from_list(pcb, &kernel_page_head, 1);
    recycle_node_from_list(pcb, &user_page_mem_head, 1);
    recycle_node_from_list(pcb, &user_page_sd_head, 0);
}
/* --------------------------------------------MAP VA to PAGE TABLE -------------------------------------------------------------------*/
/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb)  // offset of va should be 0
{
    PTE *lv3_pgdir = (PTE *) pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
    uint64_t offset = va & 0xFFF;   // first 12 bit
    if (lv3_pgdir[vpn2] == 0) {     // alloc a new second-level page directory
        PTE *lv2_pgdir = (PTE *) alloc_kernel_page();
        create_PageNode((uintptr_t) lv2_pgdir, (uintptr_t) lv2_pgdir, (uintptr_t) PGDIR_VA, pcb);
        set_pfn(&lv3_pgdir[vpn2], (uint64_t) kva2pa(lv2_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv3_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(lv2_pgdir);   // clear second-level pgdir page
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {     // alloc a new first_level page directory
        PTE *lv1_pgdir = (PTE *) alloc_kernel_page();
        create_PageNode((uintptr_t) lv1_pgdir, (uintptr_t) lv1_pgdir, (uintptr_t) PGDIR_VA, pcb);
        set_pfn(&lv2_pgdir[vpn1], kva2pa(lv1_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv2_pgdir[vpn1], _PAGE_PRESENT);
        clear_pgdir(lv1_pgdir);   // clear second-level pgdir page
    }
    PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
    uintptr_t kva = (uintptr_t) alloc_user_page();
    create_PageNode(kva, va, pcb->pgdir, pcb);
    uintptr_t upa = kva2pa(kva);
    set_pfn(&lv1_pgdir[vpn0], upa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &lv1_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_DIRTY);
    local_flush_tlb_all();
    local_flush_icache_all();
    return kva;
}
/* -----------------------------------------PAGE NODE LIST---------------------------------------------------------------------------- */
uintptr_t swap_page() {
    assert(user_page_mem_head.next != &user_page_mem_head);
    while (get_attribute(*user_page_mem_head.next->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY)) {    // the page has be accessed
        clear_attribute(user_page_mem_head.next->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY);
        forward_list_head(&user_page_mem_head);
    }
    // move the pagenode into sd_list
    PageNode_t *swapped_page = user_page_mem_head.next;
    delete_list_node(swapped_page);
    // clear_attribute(swapped_page->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY);
    *swapped_page->pte_entry = 0;
    local_flush_tlb_page(swapped_page->uva);        // refresh tlb
    local_flush_icache_all();
    uintptr_t kva = swapped_page->addr.kva;
    // write into sd card
    printl("swap out kva = %lx, uva = %lx, pid = %d\n", kva, swapped_page->uva, swapped_page->pid);
    if (swapped_page->uva == 0) {
        printl("error: swapped_page = %lx, kva = %lx, pte = %lx, stateus = %d \n", swapped_page, swapped_page->addr.kva, *swapped_page->pte_entry, swapped_page->status);
        while (1);
    }
    sd_write(kva2pa(kva), PAGE_SIZE / SECTOR_SIZE, sd_sector_end);
    swapped_page->addr.sector_id = sd_sector_end;
    sd_sector_end += PAGE_SIZE / SECTOR_SIZE;
    insert_list_tail(swapped_page, &user_page_sd_head);
    return kva;
}
void swap_page_in(PageNode_t *p) {
    assert(p->uva != 0);
    delete_list_node(p);                       // remove from sd_list
    uintptr_t kva = alloc_user_page();
    sd_read(kva2pa(kva), PAGE_SIZE / SECTOR_SIZE, p->addr.sector_id);
    printl("swap in kva = %lx, uva = %lx, pid = %d\n", kva, p->uva, p->pid);
    p->addr.kva = kva;
    set_pfn(p->pte_entry, kva2pa(kva) >> NORMAL_PAGE_SHIFT);
    set_attribute(
        p->pte_entry, _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_DIRTY);
    insert_list_tail(p, &user_page_mem_head);   // insert into mem_list
    local_flush_tlb_all();
    return;
}

void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    uint64_t vpn = stval & ~(PAGE_SIZE - 1);

    // first time to visit the page
    PageNode_t *p;
    p = search_list_node(&user_page_mem_head, vpn, current_running);
    if (p) {
        set_attribute(p->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY);
        local_flush_tlb_all();
        local_flush_icache_all();
        return;
    }
    // page is swapped to sd
    p = search_list_node(&user_page_sd_head, vpn, current_running);
    if (p) { // success to find the swapped page
        assert(p->uva != 0);
        swap_page_in(p);
        return;
    }
    // have not create page_node
    alloc_page_helper(vpn, current_running->pgdir, current_running);  // stval is va triggering exception
    return;     // jump to ret_from_exception, redo the inst
}


void create_PageNode(uintptr_t kva, uintptr_t uva, PTE *pgdir, pcb_t *pcb) {
    if (uva == 0) {
        while (1);
    }
    int i = get_free_PageNode();
    pn_list[i].status = PN_ACTIVE;
    pn_list[i].addr.kva = kva;
    pn_list[i].pid = pcb->pid;
    pn_list[i].uva = uva;
    printl("<%d> create pagenode, addr = %lx, pid = %d, uva = %d\n", i, &pn_list[i], pn_list[i].pid, pn_list[i].uva);

    if (uva & 1 << 28) {    // kernel space, 2 level page table
        uva &= VA_MASK;
        uint64_t vpn2 = uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        PTE *lv3_pgdir = pgdir;
        PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
        pn_list[i].pte_entry = &lv2_pgdir[vpn1];
        insert_list_tail(&pn_list[i], &kernel_page_head);
    } else {    // user space, 3 level page level
        uva &= VA_MASK;
        uint64_t vpn2 = uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
        PTE *lv3_pgdir = pgdir;
        PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
        PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
        pn_list[i].pte_entry = &lv1_pgdir[vpn0];
        insert_list_tail(&pn_list[i], &user_page_mem_head);
    }
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}

void check_uva_mem(uintptr_t uva_begin, uint64_t len) {
    uint64_t vpn = uva_begin & ~(PAGE_SIZE - 1);
    while (vpn <= uva_begin + len) {
        PageNode_t *p = search_list_node(&user_page_sd_head, vpn, current_running);
        if (p) {
            if (p->uva == 0) {
                while (1);   // error
            }
            swap_page_in(p);
        }
        vpn += PAGE_SIZE;
    }
}