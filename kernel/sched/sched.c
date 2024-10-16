#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/string.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t) pid0_stack,
    .user_sp = (ptr_t) pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;
/* global process id */
int pcb_id = 0;

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    if (current_running->status == TASK_RUNNING)      // put the current_runnning to the tail of ready_queue
        add_readyqueue(current_running);

    pcb_t *prepcb = current_running;

    current_running = LIST_PCB(ready_queue.next);
    ready_queue.next = ready_queue.next->next;       // delete current_running from ready_queue
    // TODO: [p2-task1] switch_to current_running
    if (current_running != LIST_PCB(&ready_queue)) {
        // if (current_running->pid == 0) {        // the first time to exec
        //     active_pcb[process_id] = current_running;
        //     current_running->pid = process_id++;
        // }
        current_running->status = TASK_RUNNING;
        switch_to(prepcb, current_running);
    } else
        while (1);
    return;             //  system_yeild(real context): return -> handle_syscall -> interrupt_helper -> ret_from_exception
}                       // or main(fake context): return -> ret_from_exception
                        // or do_mutex_lock_acquire(): return -> do_mutex_lock_acquire ->ret_from_exception
void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->wakeup_time = sleep_time + get_timer();
    do_block(&current_running->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    queue->prev->next = pcb_node;
    pcb_node->prev = queue->prev;
    pcb_node->next = queue;
    queue->prev = pcb_node;
    pcb_t *pcb = LIST_PCB(pcb_node);
    pcb->status = TASK_BLOCKED;
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    pcb_node->prev->next = pcb_node->next;          // delete the pcb from block queue
    pcb_node->next->prev = pcb_node->prev;
    pcb_t *pcb = LIST_PCB(pcb_node);
    add_readyqueue(pcb);
}

void set_sche_workload(int remain_length) {         // updata remain_length in pcb
    current_running->remain_length = remain_length;
}
void do_process_show() {
    printk("[Process Table]\n");
    for (int i = 0;i < process_id;i++) {
        printk("[%d] PID : %d   STATUS : ", i, pcb[i].pid);
        switch (pcb[i].status)
        {
        case TASK_BLOCKED:
            printk("TASK_BLOCKED\n");
            break;
        case TASK_RUNNING:
            printk("TASK_RUNNING\n");
            break;
        case TASK_READY:
            printk("TASK_READY\n");
            break;
        case TASK_EXITED:
            printk("TASK_EXITED\n");
            break;
        default:
            printk("ERROR\n");
            break;
        }
    }
}

pid_t do_exec(char *name, int argc, char *argv[]) {
    // add_readyqueue(taskname2pcb(name));'
    int taskid = taskname_to_taskid(name);
    if (taskid < 0)  return -1;
    if (pcb_id >= NUM_MAX_TASK) return -1;

    pcb_id++;
    /* init pcb */
    pcb[pcb_id].kernel_stack_base = allocKernelPage(KernelStackPage) + KernelStackPage * PAGE_SIZE;
    pcb[pcb_id].user_sp = allocUserPage(UserStackPage) + UserStackPage * PAGE_SIZE;
    pcb[pcb_id].user_stack_base = pcb[pcb_id].user_sp;
    pcb[pcb_id].pid = 0;
    pcb[pcb_id].status = TASK_EXITED;
    pcb[pcb_id].entry_point = tasks[taskid].entry;
    pcb[pcb_id].remain_length = 0;

    /* init pcb stack */
    // move args to stack
    ptr_t kernel_sp = pcb[pcb_id].kernel_stack_base - 8;    // argc_base
    *(int64_t *) kernel_sp = (int64_t) argc;
    kernel_sp = kernel_sp - 8 * argc;       // kernel_sp_argv_base
    ptr_t argv_base = kernel_sp;
    memcpy(kernel_sp, argv, 8 * argc);
    for (int i = 0; i < argc; i++) {
        int str_len = strlen(argv[i]) + 1;  // include '\0'
        kernel_sp -= str_len;
        strcpy(kernel_sp, argv[i]);
    }
    kernel_sp = ROUNDDOWN(kernel_sp, 16);  // alignment to 128 bit = 16 byte
    pcb[pcb_id].kernel_sp = kernel_sp;
    // init reg context
    regs_context_t *pt_regs =
        (regs_context_t *) (pcb[pcb_id].kernel_sp - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++) {
        if (i == 1) // ra
            pt_regs->regs[i] = pcb[pcb_id].entry_point;
        else if (i == 2) // sp
            pt_regs->regs[i] = pcb[pcb_id].user_sp;
        else if (i == 4) // tp
            pt_regs->regs[i] = (reg_t) &pcb[pcb_id];
        else if (i == 10) // a0
            pt_regs->regs[i] = argc;
        else if (i == 11) // a1
            pt_regs->regs[i] = argv_base;
        else
            pt_regs->regs[i] = 0;
    }
    pt_regs->sstatus = ((0UL & (~SR_SPP)) & (~SR_SIE)) | SR_SPIE;           // set spp = 0, spie = 1, sie = 0
    pt_regs->sepc = pcb[pcb_id].entry_point;            // entry 
    pt_regs->scause = 0UL | EXC_SYSCALL;    // IRQ 

    switchto_context_t *pt_switchto =
        (switchto_context_t *) ((ptr_t) pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++) {
        if (i == 0) { // ra
            pt_switchto->regs[i] = (reg_t) ret_from_exception;
        } else if (i == 1) {   // sp
            pt_switchto->regs[i] = pcb[pcb_id].kernel_sp;
        } else {      // S0 - S11
            pt_switchto->regs[i] = 0;
        }
    }
    pcb[pcb_id].kernel_sp = pcb[pcb_id].kernel_sp - sizeof(regs_context_t) - sizeof(switchto_context_t);
    /* add to readyqueue */
    add_readyqueue(&pcb[pcb_id]);
    pcb[pcb_id].pid = ++process_id;
    //do_scheduler();
    return process_id;

}
void add_readyqueue(pcb_t *pcb)        // add the tail of ready_queue
{
    if (!pcb)   return;  // pcb = NULL
    pcb->status = TASK_READY;
    list_node_t *p = &ready_queue;
    while (p->next != &ready_queue) {
        p = p->next;
    }
    pcb->list.next = p->next;   // &ready_queue
    p->next = &pcb->list;
}
