#include <os/mm.h>
#include <os/irq.h>
#include <assert.h>
void swap_page_in(PageNode_t *p);
/* ---------------------------------------------BIT MAP------------------------------------------------------------ */
#define NPAGES2BITMAPCH(npages) (((npages) / 8) + ((npages) % 8 != 0))
// Each bit represents the state of a page. When initialized, 
// all bits are set to 0, indicating that all pages are unallocated.
char user_bitmap[NPAGES2BITMAPCH(FREE_USER_PAGE_NUM)] = { 0 }; // Each char has 8 bits to represent 8 pages
char kernel_bitmap[NPAGES2BITMAPCH(FREE_KERNEL_PAGE_NUM)] = { 0 };

void init_mem_manager() {
    // sd buffer addr
    sd_sector_end = (uint64_t) * (short *) nsectors_image_loc;
    // PageNode list init
    pn_list = (PageNode_t *) PN_LIST_BASE;
    for (int i = 0;i < PageNode_MAXNUM;i++) {
        pn_list[i].status = PN_INACTIVE;
    }
    // list head
    init_list_head(&kernel_page_head);
    init_list_head(&user_page_mem_head);
    init_list_head(&user_page_sd_head);
    // 0xffffffc052000000 - 0xffffffc052004000 are used as stack, first 4 page
    kernel_bitmap[0] |= 0x0f;    // 0b1111
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
// alock a page of kva: 4KB for kernel, return kva  
ptr_t alloc_kernel_page()
{
    uint64_t page_idx = find_free_pages(kernel_bitmap, FREE_KERNEL_PAGE_NUM);
    if (page_idx == -1) {
        return swap_page_out();      // fail to alloc
    }
    mark_page_allocated(page_idx, kernel_bitmap);

    // return kva
    return INIT_KERNEL_STACK + PAGE_SIZE * page_idx;
}
// alock a page of kva: 4KB for user, return kva  
ptr_t alloc_user_page() {
    uint64_t page_idx = find_free_pages(user_bitmap, FREE_USER_PAGE_NUM);
    if (page_idx == -1) {
        return swap_page_out();      // fail to alloc
    }
    mark_page_allocated(page_idx, user_bitmap);

    // return kva
    return USER_MEM_BASE + PAGE_SIZE * page_idx;
}

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
    if (lv3_pgdir[vpn2] == 0) {     // alloc a new second-level page directory
        PTE *lv2_pgdir = (PTE *) alloc_kernel_page();
        create_PageNode((uintptr_t) lv2_pgdir, (uintptr_t) lv2_pgdir, (PTE *) PGDIR_VA, pcb);
        set_pfn(&lv3_pgdir[vpn2], (uint64_t) kva2pa(lv2_pgdir) >> NORMAL_PAGE_SHIFT);
        set_attribute(&lv3_pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(lv2_pgdir);   // clear second-level pgdir page
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {     // alloc a new first_level page directory
        PTE *lv1_pgdir = (PTE *) alloc_kernel_page();
        create_PageNode((uintptr_t) lv1_pgdir, (uintptr_t) lv1_pgdir, (PTE *) PGDIR_VA, pcb);
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
/* --------------------------------------------tool fuction -------------------------------------------------------------------*/
/* This is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // share_pgtable:
    memcpy((uint8_t *) dest_pgdir, (uint8_t *) src_pgdir, PAGE_SIZE); // copy kernel pgdir
}

PTE *kva2pte(uintptr_t kva, PTE *pgdir) {
    kva = kva & VA_MASK;
    uint64_t vpn2 = kva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (kva >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    PTE *lv3_pgdir = current_running->pgdir;
    if (lv3_pgdir[vpn2] == 0) {
        return NULL;
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));

    return (PTE *) (&lv2_pgdir[vpn1]);
}
PTE *uva2pte(uintptr_t uva, PTE *pgdir) {
    PTE *lv3_pgdir = (PTE *) pgdir;
    uva &= VA_MASK;
    uint64_t vpn2 = uva >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (uva >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (uva >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);

    if (lv3_pgdir[vpn2] == 0) {
        return NULL;
    }
    PTE *lv2_pgdir = (PTE *) pa2kva(get_pa(lv3_pgdir[vpn2]));
    if (lv2_pgdir[vpn1] == 0) {
        return NULL;
    }
    PTE *lv1_pgdir = (PTE *) pa2kva(get_pa(lv2_pgdir[vpn1]));
    return (PTE *) (&lv1_pgdir[vpn0]);
}
void create_PageNode(uintptr_t kva, uintptr_t uva, PTE *pgdir, pcb_t *pcb) {
    assert(uva != 0);
    int i = get_free_PageNode();
    pn_list[i].status = PN_ACTIVE;
    pn_list[i].addr.kva = kva;
    pn_list[i].pid = pcb->pid;
    pn_list[i].uva = uva;
    printl("<%d> create pagenode, addr = %lx, pid = %d, uva = %lx, kva = %lx\n", i, &pn_list[i], pn_list[i].pid, pn_list[i].uva, pn_list[i].addr.kva);

    if (uva == kva) {    // kernel space, 2 level page table
        pn_list[i].pte_entry = kva2pte(kva, pgdir);
        insert_list_tail(&pn_list[i], &kernel_page_head);
    } else {    // user space, 3 level page level
        pn_list[i].pte_entry = uva2pte(uva, pgdir);
        insert_list_tail(&pn_list[i], &user_page_mem_head);
    }
}
// check the uva is in mem when kernel visit it
void check_uva_mem(uintptr_t uva_begin, uint64_t len, pcb_t *pcb) {
    uint64_t vpn = uva_begin & ~(PAGE_SIZE - 1);
    while (vpn <= uva_begin + len) {
        PageNode_t *p = search_list_node(&user_page_sd_head, vpn, pcb);
        if (p) {
            if (p->uva == 0) {
                while (1);   // error
            }
            swap_page_in(p);
        }
        vpn += PAGE_SIZE;
    }
}
/* -----------------------------------------PAGE Management--------------------------------------------------------------------------- */
// swap out a page, return the kva
uintptr_t swap_page_out() {
    assert(user_page_mem_head.next != &user_page_mem_head);
    // select a page in user_page_mem
    while (get_attribute(*user_page_mem_head.next->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY)) {    // the page has be accessed
        clear_attribute(user_page_mem_head.next->pte_entry, _PAGE_ACCESSED | _PAGE_DIRTY);
        forward_list_head(&user_page_mem_head);
    }
    // move the pagenode into sd_list
    PageNode_t *swapped_page = user_page_mem_head.next;
    delete_list_node(swapped_page);
    clear_attribute(swapped_page->pte_entry, _PAGE_PRESENT);
    local_flush_tlb_page(swapped_page->uva);        // refresh tlb
    uintptr_t kva = swapped_page->addr.kva;

    printl("swap out kva = %lx, uva = %lx, pid = %d\n", kva, swapped_page->uva, swapped_page->pid);
    assert(swapped_page->uva != 0);
    // write into sd card
    sd_write(kva2pa(kva), PAGE_SIZE / SECTOR_SIZE, sd_sector_end);
    swapped_page->addr.sector_id = sd_sector_end;
    sd_sector_end += PAGE_SIZE / SECTOR_SIZE;
    insert_list_tail(swapped_page, &user_page_sd_head);
    return kva;
}
// swap in a page
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
    assert(stval != 0);
    uint64_t vpn = stval & ~(PAGE_SIZE - 1);
    if (check_mprotect(regs, stval, scause) == 1) {
        return;
    }
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
    if (check_brk(vpn) == 1) {
        return;
    }
    alloc_page_helper(vpn, current_running->pgdir, current_running);  // stval is va triggering exception
    return;     // jump to ret_from_exception, redo the inst
}
/* -------------------------------------map va to pa ----------------------------------------------------------------- */
int umap_page(uintptr_t va, uintptr_t pa, uintptr_t pgdir, pcb_t *pcb) {
    PTE *lv3_pgdir = (PTE *) pgdir;
    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) ^ (vpn2 << (2 * PPN_BITS)) ^ (vpn1 << PPN_BITS);
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
    uintptr_t kva = pa2kva(pa);
    create_PageNode(kva, va, pcb->pgdir, pcb);
    // uintptr_t upa = kva2pa(kva);
    set_pfn(&lv1_pgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &lv1_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
        _PAGE_EXEC | _PAGE_USER | _PAGE_DIRTY);
    local_flush_tlb_all();
    return 1;
}
/* --------------------------------------------------------shmpage ------------------------------------------------------ */
static ptr_t userMemCurr = USER_STACK_ADDR;

ptr_t allocFreeUva(int numPage) {
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}
void init_shmpage() {
    for (int i = 0; i < SHM_PAGE_NUM; i++) {
        shm_pages[i].key = 0;
        shm_pages[i].status = SHM_UNUSED;
        shm_pages[i].user_num = 0;
    }
}
uintptr_t shm_page_get(int key)
{
    int i;
    uintptr_t uva;

    for (i = 0; i < SHM_PAGE_NUM; i++) {
        // the key is not the first time to use
        if (shm_pages[i].status == SHM_USING && shm_pages[i].key == key) {
            do {
                uva = allocFreeUva(1);
            } while (uva2pte(uva, current_running->pgdir) != NULL);
            umap_page(uva, kva2pa(shm_pages[i].kva), current_running->pgdir, current_running);
            shm_pages[i].user_num++;
            return uva;
        }
    }
    // the key is the fisrt time to use
    for (i = 0; i < SHM_PAGE_NUM; i++) {
        if (shm_pages[i].status == SHM_UNUSED) {
            break;
        }
    }
    do {
        uva = allocFreeUva(1);
    } while ((uva, current_running->pgdir) != NULL);

    shm_pages[i].kva = alloc_page_helper(uva, current_running->pgdir, current_running);
    shm_pages[i].status = SHM_USING;
    shm_pages[i].key = key;
    shm_pages[i].user_num++;
    return uva;
}
// unmap the user pages
static inline void delete_uva_page(uintptr_t vpn, pcb_t *pcb) {
    PageNode_t *p = search_list_node(&user_page_mem_head, vpn, pcb);
    if (p) {
        uint64_t page_idx = (p->addr.kva - USER_MEM_BASE) / PAGE_SIZE;
        unmark_page_free(page_idx, user_bitmap);
        p->status = PN_INACTIVE;
        delete_list_node(p);
    }
    p = search_list_node(&user_page_sd_head, vpn, pcb);
    if (p) {
        p->status = PN_INACTIVE;
        delete_list_node(p);
    }
}
void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
    uintptr_t uva = addr & VA_MASK;
    PTE *pte = uva2pte(uva, current_running->pgdir);
    uint64_t pa = get_pa(*pte);
    uint64_t kva = pa2kva(pa);

    for (int i = 0; i < SHM_PAGE_NUM; i++) {
        if (shm_pages[i].status == SHM_USING && shm_pages[i].kva == kva) {
            // delete pagenode
            uint64_t  vpn = uva & ~(PAGE_SIZE - 1);
            delete_uva_page(vpn, current_running);
            // unmap 
            clear_attribute(pte, _PAGE_PRESENT);
            local_flush_tlb_all();
            shm_pages[i].user_num--;
            // no thread use it
            if (shm_pages[i].user_num == 0) {
                shm_pages[i].status = SHM_UNUSED;
                shm_pages[i].key = 0;
                shm_pages[i].kva = 0;
            }
            return;
        }
    }
}

/* ------------------------------------------------mprotect -------------------------------------------------------*/
int do_mprotect(void *addr, size_t len, int prot) {
    if (((uintptr_t) addr & 0xFFF) != 0) { // not aligned to page bound
        return -1;
    }
    int n_page = NBYTES2PAGE(len);
    for (int i = 0;i < n_page;i++) {
        uintptr_t uva = (uintptr_t) addr + i * PAGE_SIZE;
        PTE *pte = uva2pte(uva, current_running->pgdir);
        if (prot == PROT_NONE) {
            clear_attribute(pte, _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC);
        } else if (prot == PROT_READ) {
            clear_attribute(pte, _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC);
            set_attribute(pte, _PAGE_READ);
        } else if (prot == PROT_WRITE) {
            clear_attribute(pte, _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC);
            set_attribute(pte, _PAGE_WRITE);
        } else if (prot == PROT_EXEC) {
            clear_attribute(pte, _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC);
            set_attribute(pte, _PAGE_EXEC);
        } else {
            return -1;
        }
    }
    local_flush_tlb_all();
    return 0;
}
int check_mprotect(regs_context_t *regs, uint64_t stval, uint64_t scause) {
    PTE *pte;
    if (pte = uva2pte(stval, current_running->pgdir) == NULL) {     // have no pte
        return 0;
    }

    if (((scause & SCAUSE_EXC_CODE) == EXCC_INST_PAGE_FAULT) && get_attribute(*pte, _PAGE_EXEC) == 0) {
        printk("ERROR: mprotect: addr = 0x%lx, cannot exe", stval);
    } else if (((scause & SCAUSE_EXC_CODE) == EXCC_LOAD_PAGE_FAULT) && get_attribute(*pte, _PAGE_READ) == 0) {
        printk("ERROR: mprotect: addr = 0x%lx, cannot read", stval);
    } else if (((scause & SCAUSE_EXC_CODE) == EXCC_STORE_PAGE_FAULT) && get_attribute(*pte, _PAGE_WRITE) == 0) {
        printk("ERROR: mprotect: addr = 0x%lx, cannot write", stval);
    } else {
        return 0;
    }
    do_exit();
    return 1;
}

/* ------------------------------------------brk/sbrk----------------------------------------------------- */
void do_getbrk(uint64_t bss_end) {
    current_running->brk = ROUNDDOWN(bss_end + PAGE_SIZE, PAGE_SIZE);
}
int check_brk(uintptr_t vpn) {
    if (vpn > current_running->brk && vpn < USER_STACK_END) {
        printk("ERROR: segment fault");
        do_exit();
        return 1;
    } else {
        return 0;
    }
}
int do_brk(void *addr) {
    uintptr_t vpn = (uintptr_t) addr & ~(PAGE_SIZE - 1);;
    if (vpn > current_running->brk && vpn < USER_STACK_END) {
        current_running->brk = (uintptr_t) addr;
        return 0;
    } else {
        return -1;
    }
}
void *do_sbrk(intptr_t increment) {
    increment = ROUND(increment, PAGE_SIZE);
    if (current_running->brk + increment < USER_STACK_END) {
        intptr_t old_brk = current_running->brk;
        current_running->brk += increment;
        return old_brk;
    } else {
        return (void *) -1;
    }
}