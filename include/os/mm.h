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

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define FREE_MEM_PAGE_NUM 0xd000    // (0x5f00_0000 - 0x5200_0000) / 4K = 0xd00_0000/ 0x1000 = 0xd000
#define PAGE_SIZE 4096 // 4K = 0x1000
#define INIT_KERNEL_STACK 0xffffffc052000000
#define FREEMEM_KERNEL (INIT_KERNEL_STACK + 4*PAGE_SIZE)

 /* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define NBYTES2PAGE(nbytes) (((nbytes) / PAGE_SIZE) + ((nbytes) % PAGE_SIZE != 0))

extern ptr_t allocPage(int numPage, pcb_t *pcb);

#define USER_STACK_ADDR 0xf00010000     // user sp : 0xf_0000_f000 - 0xf_0001_0000


// TODO [P4-task1] */
extern void init_bitmap();
extern void release_process_page(pcb_t *pcb);
extern void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir);
extern uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir, pcb_t *pcb);

typedef enum {
    PN_ACTIVE,
    PN_INACTIVE,
} PageNode_status_t;

typedef struct PageNode {
    struct PageNode *next;
    struct PageNode *prev;

    PageNode_status_t PN_status;

    PTE *pte_entry;
    pcb_t *master_pcb;

    int page_idx;   // in order to calcu kva
}PageNode_t;

PageNode_t *PageNode = 0xffffffc05f000000ul;
#define PageNode_MAXNUM 0x40000

PageNode_t *PN_clock_ptr = NULL;

// TODO [P4-task4]: shm_page_get/dt */
uintptr_t shm_page_get(int key);
void shm_page_dt(uintptr_t addr);



#endif /* MM_H */
