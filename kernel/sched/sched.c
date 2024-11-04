#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/string.h>
#include <os/smp.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;     // master kernel 
const ptr_t pid1_stack = INIT_KERNEL_STACK + PAGE_SIZE * 3;            // slave kernel
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t) pid0_stack,
    .user_sp = (ptr_t) pid0_stack + PAGE_SIZE,
    .cpu_mask = 0x1
};
pcb_t pid1_pcb = {
    .pid = 1,
    .kernel_sp = (ptr_t) pid1_stack,
    .user_sp = (ptr_t) pid1_stack + PAGE_SIZE,
    .cpu_mask = 0x2
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 2;

int get_next_running() {
    lock_kernel(&ready_queue_hart_lock);          // lock for ready_queue
    lock_kernel(&pcb_pid_hart_lock);

    if (current_running->status == TASK_RUNNING)      // put the current_runnning to the tail of ready_queue
        add_readyqueue(current_running);

    uint64_t hartid = get_current_cpu_id();
    list_node_t *next_running_list = ready_queue.next;
    pcb_t *next_running;
    while (next_running_list != &ready_queue) {
        next_running = LIST_PCB(next_running_list);
        if (next_running->cpu_mask & 1UL << hartid) {
            current_running = next_running;
            remove_readyqueue(current_running);
            current_running->status = TASK_RUNNING;
            current_running->current_cpu_id = get_current_cpu_id();
            unlock_kernel(&ready_queue_hart_lock);
            unlock_kernel(&pcb_pid_hart_lock);
            return 1;
        }
        next_running_list = next_running_list->next;
    }
    unlock_kernel(&pcb_pid_hart_lock);
    unlock_kernel(&ready_queue_hart_lock);
    return 0;   // fail to get next_running
}
void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    // TODO: [p2-task1] Modify the current_running pointer.

    pcb_t *prepcb = current_running;
    if (get_next_running()) {
        switch_to(prepcb, current_running);
        return;
    } else {
        if (get_current_cpu_id() == 0) {
            current_running = &pid0_pcb;
            switch_to(prepcb, current_running);
        } else {
            current_running = &pid1_pcb;
            switch_to(prepcb, current_running);
        }
        return;
    }

}
void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    lock_kernel(&sleep_queue_lock);
    current_running->wakeup_time = sleep_time + get_timer();
    do_block(&current_running->list, &sleep_queue);
    unlock_kernel(&sleep_queue_lock);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    // lock_kernel(&block_queue_lock);
    list_node_t *head = queue;
    list_node_t *tail = queue->prev;
    tail->next = pcb_node;
    pcb_node->prev = tail;
    head->prev = pcb_node;
    pcb_node->next = head;

    pcb_t *pcb = LIST_PCB(pcb_node);
    pcb->status = TASK_BLOCKED;
    // unlock_kernel(&block_queue_lock);
}

void do_unblock(list_node_t *pcb_node)
{
    // lock_kernel(&block_queue_lock);
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    list_node_t *next_node = pcb_node->next;        // delete the pcb from block queue
    list_node_t *prev_node = pcb_node->prev;
    pcb_node->next = NULL;
    pcb_node->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;

    pcb_t *pcb = LIST_PCB(pcb_node);
    lock_kernel(&ready_queue_hart_lock);
    add_readyqueue(pcb);
    unlock_kernel(&ready_queue_hart_lock);
    // unlock_kernel(&block_queue_lock);
}

void set_sche_workload(int remain_length) {         // updata remain_length in pcb
    current_running->remain_length = remain_length;
}
/* --------------------------------------process show----------------------------------------------------- */
void do_process_show() {
    lock_kernel(&pcb_pid_hart_lock);
    printk("[Process Table]\n");
    for (int i = 0;i < NUM_MAX_TASK;i++) {
        if (pcb[i].status != TASK_EXITED) {
            printk("[%d] PID : %d   STATUS :", i, pcb[i].pid);
            switch (pcb[i].status)
            {
            case TASK_BLOCKED:
                printk("BLOCKED\t");
                printk("mask: %d\t", pcb[i].cpu_mask);
                break;
            case TASK_RUNNING:
                printk("RUNNING\t");
                printk("mask: %d\t", pcb[i].cpu_mask);
                printk("Running on core %d\t", pcb[i].current_cpu_id);
                break;
            case TASK_READY:
                printk("READY\t");
                printk("mask: %d\t", pcb[i].cpu_mask);
                break;
            default:
                printk("ERROR\t");
                break;
            }
            printk("\n");
        }
    }
    unlock_kernel(&pcb_pid_hart_lock);
}
/*---------------------------------ready queue management ----------------------------------------------*/
void add_readyqueue(pcb_t *pcb)        // add the tail of ready_queue
{
    if (!pcb)   return;  // pcb = NULL
    pcb->status = TASK_READY;
    list_node_t *head = &ready_queue;
    list_node_t *tail = ready_queue.prev;
    tail->next = &pcb->list;
    pcb->list.prev = tail;
    head->prev = &pcb->list;
    pcb->list.next = head;
}
void remove_readyqueue(pcb_t *pcb) {
    if (!pcb)   return;  // pcb = NULL
    list_node_t *pcb_list = &pcb->list;
    list_node_t *next_node = pcb_list->next;
    list_node_t *prev_node = pcb_list->prev;
    pcb_list->next = NULL;
    pcb_list->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}
/*---------------------------------exec, kill, exit, waitpid --------------------------------------------------*/
pid_t do_exec(char *name, int argc, char *argv[]) {
    lock_kernel(&pcb_pid_hart_lock);
    int taskid = taskname_to_taskid(name);
    if (taskid < 0) {
        unlock_kernel(&pcb_pid_hart_lock);
        return -1;
    }
    int pcb_id = 0;
    for (; pcb_id < NUM_MAX_TASK;pcb_id++) {
        if (pcb[pcb_id].status == TASK_EXITED) {
            break;
        }
    }
    if (pcb_id >= NUM_MAX_TASK) {
        unlock_kernel(&pcb_pid_hart_lock);
        return -1; // pcb has run out
    }
    /* init pcb */
    pcb[pcb_id].kernel_sp = allocKernelSP();
    pcb[pcb_id].kernel_stack_base = pcb[pcb_id].kernel_sp;
    pcb[pcb_id].user_sp = allockUserSP();
    pcb[pcb_id].user_stack_base = pcb[pcb_id].user_sp;
    int pid = ++process_id;
    pcb[pcb_id].pid = pid;
    pcb[pcb_id].entry_point = tasks[taskid].entry;
    pcb[pcb_id].remain_length = 0;
    pcb[pcb_id].block_queue.next = &pcb[pcb_id].block_queue;
    pcb[pcb_id].block_queue.prev = &pcb[pcb_id].block_queue;
    pcb[pcb_id].cpu_mask = current_running->cpu_mask;
    strcpy(pcb[pcb_id].taskname, name);

    /* init pcb stack */
    // move args to stack
    ptr_t user_sp = pcb[pcb_id].user_stack_base - 8;    // argc_base
    *(int64_t *) user_sp = (int64_t) argc;
    user_sp = user_sp - 8 * argc;       // kernel_sp_argv_base
    ptr_t argv_base = user_sp;
    char **my_argv = (char **) argv_base;
    // memcpy((uint8_t *) user_sp, (const uint8_t *) argv, 8 * argc);
    for (int i = 0; i < argc; i++) {
        int str_len = strlen(argv[i]) + 1;  // include '\0'
        user_sp -= str_len;
        my_argv[i] = (char *) user_sp;
        strcpy((char *) user_sp, argv[i]);
    }
    user_sp = ROUNDDOWN(user_sp, 16);  // alignment to 128 bit = 16 byte
    pcb[pcb_id].user_sp = user_sp;
    // init reg context
    ptr_t kernel_sp = pcb[pcb_id].kernel_stack_base;
    pcb[pcb_id].kernel_sp = pcb[pcb_id].kernel_sp - sizeof(regs_context_t) - sizeof(switchto_context_t);
    regs_context_t *pt_regs =
        (regs_context_t *) (kernel_sp - sizeof(regs_context_t));
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
    /* add to readyqueue */
    lock_kernel(&ready_queue_hart_lock);
    add_readyqueue(&pcb[pcb_id]);
    unlock_kernel(&ready_queue_hart_lock);
    unlock_kernel(&pcb_pid_hart_lock);
    return pid;

}
int do_kill(pid_t pid) {
    lock_kernel(&pcb_pid_hart_lock);
    // ----------pid to pcb_id
    int i = 0;
    for (;i < NUM_MAX_TASK;i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            break;
        }
    }
    if (i >= NUM_MAX_TASK) {
        unlock_kernel(&pcb_pid_hart_lock);
        return 0;
    }  // fail to find
    // ------------kill itself, then go to exit
    if (&pcb[i] == current_running) {
        unlock_kernel(&pcb_pid_hart_lock);
        do_exit();
        return 1;
    }
    // ----------wake up block queue
    while (pcb[i].block_queue.next != &pcb[i].block_queue) {
        do_unblock(pcb[i].block_queue.next);
    }
    // -----------release lock
    check_lock(pid);
    // ------------recycle stack
    recycle_kernel_sp[recycle_kernel_sp_num++] = pcb[i].kernel_stack_base;
    recycle_user_sp[recycle_user_sp_num++] = pcb[i].user_stack_base;
    // -------------recycle pcb
    pcb[i].status = TASK_EXITED;
    remove_pcb_queue(&pcb[i]);
    unlock_kernel(&pcb_pid_hart_lock);
    return 1;
}
void do_exit(void) {
    lock_kernel(&pcb_pid_hart_lock);
    // wake up wait queue
    while (current_running->block_queue.next != &current_running->block_queue) {
        do_unblock(current_running->block_queue.next);
    }
    // release lock
    check_lock(do_getpid());
    // recycle stack
    recycle_kernel_sp[recycle_kernel_sp_num++] = current_running->kernel_stack_base;
    recycle_user_sp[recycle_user_sp_num++] = current_running->user_stack_base;
    // recycle pcb
    current_running->status = TASK_EXITED;
    unlock_kernel(&pcb_pid_hart_lock);
    do_scheduler();
}
int do_waitpid(pid_t pid) {
    lock_kernel(&pcb_pid_hart_lock);
    int i = 0;
    for (;i < NUM_MAX_TASK;i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            break;
        }
    }
    if (i >= NUM_MAX_TASK) {
        unlock_kernel(&pcb_pid_hart_lock);
        return 0;
    }  // fail to find
    if (pcb[i].status != TASK_EXITED) {
        do_block(&current_running->list, &pcb[i].block_queue);
        unlock_kernel(&pcb_pid_hart_lock);
        do_scheduler();
    } else {
        unlock_kernel(&pcb_pid_hart_lock);
    }
    return i;
}
pid_t do_getpid() {
    return current_running->pid;
}

int do_taskset(char *name, pid_t pid, uint64_t mask, int mod) {
    if (mod == 0) { // taskset mask taskname
        char *argv[1];
        argv[0] = name;
        int pid = do_exec(name, 1, argv);
        if (pid == -1) {
            return -1;
        }
        lock_kernel(&pcb_pid_hart_lock);
        int i = 0;
        for (;i < NUM_MAX_TASK;i++) {
            if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
                break;
            }
        }
        pcb[i].cpu_mask = mask;
        unlock_kernel(&pcb_pid_hart_lock);
    } else {// taskset -p mask pid
        lock_kernel(&pcb_pid_hart_lock);
        int i = 0;
        for (;i < NUM_MAX_TASK;i++) {
            if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
                break;
            }
        }
        if (i >= NUM_MAX_TASK) {
            unlock_kernel(&pcb_pid_hart_lock);
            return -1;
        }
        pcb[i].cpu_mask = mask;
        unlock_kernel(&pcb_pid_hart_lock);
    }
    return 0;
}
void remove_pcb_queue(pcb_t *pcb) {
    if (pcb->list.next == NULL || pcb->list.next == NULL) {
        return;
    }
    lock_kernel(&ready_queue_hart_lock);
    lock_kernel(&mutex_hart_lock);
    lock_kernel(&barrier_hart_lock);
    lock_kernel(&condition_hart_lock);
    lock_kernel(&mailbox_hart_lock);
    lock_kernel(&sleep_queue_lock);

    list_node_t *pcb_list = &pcb->list;
    list_node_t *next_node = pcb_list->next;
    list_node_t *prev_node = pcb_list->prev;
    pcb_list->next = NULL;
    pcb_list->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;

    unlock_kernel(&ready_queue_hart_lock);
    unlock_kernel(&mutex_hart_lock);
    unlock_kernel(&barrier_hart_lock);
    unlock_kernel(&condition_hart_lock);
    unlock_kernel(&mailbox_hart_lock);
    unlock_kernel(&sleep_queue_lock);
}