/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
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

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/task.h>
#include <csr.h>

#define NUM_MAX_TASK 16

 /* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32];

    /* Saved special registers. */
    reg_t sstatus;
    reg_t sepc;
    reg_t sbadaddr;
    reg_t scause;
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,
    TASK_RUNNING,
    TASK_READY,
    TASK_EXITED
} task_status_t;

typedef struct page_occupied {
    int start_page;
    int numPage;
}page_occupied_t;
typedef enum {
    MTHREAD,
    PTHREAD
}thread_type_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;
    reg_t user_sp;
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    /* previous, next pointer */
    list_node_t list;
    list_head wait_list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status;

    /* cursor position */
    int cursor_x;
    int cursor_y;
    int remain_length;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    /* enter_point of user */
    uint64_t entry_point;

    /* name of task */
    char taskname[MAX_NAME_LEN];

    /* sys_wait (pid) */
    list_head block_queue;

    /* cpu hart id relevant*/
    uint64_t current_cpu_id;
    uint64_t cpu_mask;

    /* page table */
    uintptr_t pgdir;        // the address of the base of page table

    /* MTHREAD | PTHREAD*/
    thread_type_t thread_type;
    pthread_t pthread_id;

    uintptr_t brk;

} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
pcb_t *current_running_0;
pcb_t *current_running_1;
register pcb_t *current_running asm("tp");

extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK];
extern pcb_t slave_pid0_pcb;
extern pcb_t master_pid0_pcb;
extern const ptr_t pid0_stack;
extern const ptr_t pid1_stack;

extern void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *pcb_node, list_head *queue);
void do_unblock(list_node_t *pcb_node);

extern void add_readyqueue(pcb_t *pcb);
extern void remove_readyqueue(pcb_t *pcb);
extern void remove_pcb_queue(pcb_t *pcb);

void set_sche_workload(int remain_length);

extern void ret_from_exception();
extern int taskname_to_taskid(char taskname[]);
extern int do_taskset(char *name, pid_t pid, uint64_t mask, int mod);
/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
/************************************************************/

extern void do_pthread_create(pthread_t thread, void (*start_routine)(void *), void *arg, uint64_t exit_funt);
extern void do_pthread_join(pthread_t thread);
#endif
