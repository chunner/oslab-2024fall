/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <type.h>
#include <pgtable.h>
#include <os/list.h>
#include <os/sched.h>
#include <os/string.h>
#include <common.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 4096 // 4K = 0x1000
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK + 4*PAGE_SIZE)
#define FREE_KERNEL_PAGE_NUM 0x2000

#define USER_MEM_BASE 0xffffffc054000000
#define FREE_USER_PAGE_NUM 0x8    // (0x5f00_0000 - 0x5200_0000) / 4K = 0xd00_0000/ 0x1000 = 0xd000

 /* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define NBYTES2PAGE(nbytes) (((nbytes) / PAGE_SIZE) + ((nbytes) % PAGE_SIZE != 0))

#define USER_STACK_ADDR 0xf00010000     // user sp : 0xf_0000_f000 - 0xf_0001_0000
#define nsectors_image_loc 0xffffffc0502001f2

// TODO [P4-task1] */
extern ptr_t alloc_kernel_page();
extern ptr_t alloc_user_page();
extern void release_process_page(pcb_t *pcb);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb);
extern void handle_page_fault(regs_context_t *regs, uint64_t stval, uint64_t scause);

extern void create_pn(uintptr_t kva, uintptr_t uva, PTE *pgdir, pcb_t *pcb);
extern void init_mem_manager();
uintptr_t swap_page();
extern void mark_page_allocated(int page_idx, char bitmap[]);
extern void unmark_page_free(uint64_t kva, char bitmap[]);

uint64_t sd_sector_end;

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);

/* -----------------------------------------PageNode ------------------------------------------------------- */
typedef enum {
    PN_ACTIVE,
    PN_INACTIVE,
} PageNode_status_t;

typedef struct PageNode {
    struct PageNode *next;
    struct PageNode *prev;

    PageNode_status_t status;

    PTE *pte_entry;
    pcb_t *master_pcb;

    union {
        uintptr_t kva;      // In Mem
        uint64_t sector_id; // In SDcard
    }addr;

    uintptr_t uva;
}PageNode_t;

PageNode_t *pn_list;
#define PageNode_MAXNUM 0x40000
#define PN_LIST_BASE 0xffffffc05f000000ul

PageNode_t kernel_page_head;

PageNode_t user_page_mem_head;
PageNode_t user_page_sd_head;

// List management
static inline void init_list_head(PageNode_t *head) {
    head->next = head;
    head->prev = head;
}
static inline void delete_list_node(PageNode_t *p) {
    p->prev->next = p->next;
    p->next->prev = p->prev;
    p->next = NULL;
    p->prev = NULL;
}
static inline void insert_list_tail(PageNode_t *p, PageNode_t *head) {
    p->next = head;
    p->prev = head->prev;
    head->prev->next = p;
    head->prev = p;
}
static inline void forward_list_head(PageNode_t *head) {
    if (head->next == head) return; // list is empty
    // exchange node2 and head
    PageNode_t *node1 = head->prev;
    PageNode_t *node2 = head->next;
    PageNode_t *node3 = head->next->next;

    head->next = node3;
    node3->prev = head;
    head->prev = node2;
    node2->next = head;
    node2->prev = node1;
    node1->next = node2;
}

// memory page management
static inline void recycle_node_from_list(pcb_t *pcb, PageNode_t *head, char bitmap[], int is_in_mem) {
    PageNode_t *p = head->next;
    while (p != head) {
        PageNode_t *pnext = p->next;
        if (p->master_pcb == pcb) {
            // recycle PageNode
            p->status = PN_INACTIVE;
            // recycle MemPage
            if (is_in_mem) {
                unmark_page_free(p->addr.kva, bitmap);
            }
            // remove from the list
            delete_list_node(p);
        }
        p = pnext;
    }
}

static inline int get_free_PageNode() {
    int i = 0;
    for (; i < PageNode_MAXNUM;i++) {
        if (pn_list[i].status == PN_INACTIVE) {
            return i;  // success to find
        }
    }
    return -1;  // fail to find
}

static inline void create_PageNode(uintptr_t kva, uintptr_t uva, PTE *pgdir, pcb_t *pcb) {
    int i = get_free_PageNode();
    pn_list[i].status = PN_ACTIVE;
    pn_list[i].addr.kva = kva;
    pn_list[i].master_pcb = pcb;
    pn_list[i].uva = uva;

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

static inline PageNode_t *search_list_node(PageNode_t *head, uintptr_t uva) {
    PageNode_t *p = head->next;
    while (p != head) {
        if (p->uva == uva) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}
#endif /* MM_H */
