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
#include <os/loader.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;     // master kernel 
const ptr_t pid1_stack = INIT_KERNEL_STACK + PAGE_SIZE * 3;            // slave kernel
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t) pid0_stack,
    .user_sp = (ptr_t) pid0_stack + PAGE_SIZE,
    .cpu_mask = 0x1,
    .pgdir = PGDIR_VA
};
pcb_t pid1_pcb = {
    .pid = 1,
    .kernel_sp = (ptr_t) pid1_stack,
    .user_sp = (ptr_t) pid1_stack + PAGE_SIZE,
    .cpu_mask = 0x2,
    .pgdir = PGDIR_VA
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;
/*=======================================do_scheduler=======================================================*/
int get_next_running() {    //  Modify the current_running pointer.
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
            return 1;
        }
        next_running_list = next_running_list->next;
    }
    return 0;   // fail to get next_running
}
void switch_to_current_satp(void) {
    set_satp(SATP_MODE_SV39, current_running->pid, kva2pa(current_running->pgdir) >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();
    return;
}
void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
    pcb_t *prepcb = current_running;
    if (get_next_running()) {
        switch_to_current_satp();
        switch_to(prepcb, current_running);
        return;
    } else {
        if (get_current_cpu_id() == 0) {
            current_running = &pid0_pcb;
            switch_to_current_satp();
            switch_to(prepcb, current_running);
        } else {
            current_running = &pid1_pcb;
            switch_to_current_satp();
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
    current_running->wakeup_time = sleep_time + get_timer();
    do_block(&current_running->list, &sleep_queue);
    do_scheduler();
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    list_node_t *head = queue;
    list_node_t *tail = queue->prev;
    // insert pcb_node in the circular queue
    tail->next = pcb_node;
    pcb_node->prev = tail;
    head->prev = pcb_node;
    pcb_node->next = head;
    // modify the pcb task_status
    pcb_t *pcb = LIST_PCB(pcb_node);
    pcb->status = TASK_BLOCKED;
}

void do_unblock(list_node_t *pcb_node)
{
    // unblock the `pcb` from the block queue
    list_node_t *next_node = pcb_node->next;
    list_node_t *prev_node = pcb_node->prev;
    // delete the pcb from block queue (circular queue)
    pcb_node->next = NULL;
    pcb_node->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;
    // insert pcb_node in the circular queue
    pcb_t *pcb = LIST_PCB(pcb_node);
    add_readyqueue(pcb);
}

void set_sche_workload(int remain_length) {         // updata remain_length in pcb
    current_running->remain_length = remain_length;
}
/*---------------------------------ready queue management ----------------------------------------------*/
void add_readyqueue(pcb_t *pcb)        // add the tail of ready_queue
{
    pcb->status = TASK_READY;
    list_node_t *head = &ready_queue;
    list_node_t *tail = ready_queue.prev;
    tail->next = &pcb->list;
    pcb->list.prev = tail;
    head->prev = &pcb->list;
    pcb->list.next = head;
}
void remove_readyqueue(pcb_t *pcb) {
    list_node_t *pcb_list = &pcb->list;
    list_node_t *next_node = pcb_list->next;
    list_node_t *prev_node = pcb_list->prev;
    pcb_list->next = NULL;
    pcb_list->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}
/* ==========================================process show===================================================== */
void do_process_show() {
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
}
/*-----------------------------tool function to search in arrays--------------------------------------*/
int taskname_to_taskid(char taskname[]) {
    int i = 0;
    for (; i < tasknum; i++) {
        if (strcmp(taskname, tasks[i].taskname) == 0) {
            return i;
        }
    }
    return -1;  // task name match failed
}
int pid_to_pcb_id(pid_t pid) {
    int i = 0;
    for (;i < NUM_MAX_TASK;i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            return i;
        }
    }
    return -1;
}
int get_free_pcb() {
    int pcb_id = 0;
    for (; pcb_id < NUM_MAX_TASK;pcb_id++) {
        if (pcb[pcb_id].status == TASK_EXITED) {
            return pcb_id;  // success to find
        }
    }
    return -1;  // fail to find
}
/*---------------------------------exec, kill, exit, waitpid --------------------------------------------------*/
void setup_process_pcb(pcb_t *pcb, task_info_t task) {
    pcb->kernel_sp = alloc_kernel_page() + PAGE_SIZE;
    create_pn(pcb->kernel_sp - PAGE_SIZE, pcb->kernel_sp - PAGE_SIZE, (uintptr_t) PGDIR_VA, pcb);
    pcb->kernel_stack_base = pcb->kernel_sp;
    pcb->user_sp = USER_STACK_ADDR;
    pcb->user_stack_base = USER_STACK_ADDR;
    pcb->pid = ++process_id;
    pcb->entry_point = task.entrypoint;
    pcb->block_queue.next = &pcb->block_queue;
    pcb->block_queue.prev = &pcb->block_queue;
    pcb->cpu_mask = current_running->cpu_mask;
    //pcb->page_occupied_pointer = 0;
    strcpy(pcb->taskname, task.taskname);
}

void setup_process_stack(pcb_t *pcb, int argc, char *argv[]) {
    /* -----------------------------------USER STACK--------------------------------------*/
    uint64_t user_stack_top = USER_STACK_ADDR - PAGE_SIZE;       // user sp : 0xf_000_f000 - 0xf_0001_0000
    uint64_t kva = alloc_page_helper(user_stack_top, pcb->pgdir, pcb);   // alloc and map user stack
    ptr_t user_sp_kva = kva + PAGE_SIZE;
    uint64_t user_sp_kva_uva_offset = user_sp_kva - USER_STACK_ADDR;

    user_sp_kva -= sizeof(int64_t);    // argc_base
    *(int64_t *) user_sp_kva = (int64_t) argc;

    user_sp_kva = user_sp_kva - sizeof(char *) * argc;       // kernel_sp_argv_base
    uint64_t argv_base = user_sp_kva - user_sp_kva_uva_offset;    //
    char **my_argv = (char **) user_sp_kva;
    for (int i = 0; i < argc; i++) {
        int str_len = strlen(argv[i]) + 1;  // include '\0'
        user_sp_kva -= str_len;
        my_argv[i] = (char *) (user_sp_kva - user_sp_kva_uva_offset);
        strcpy((char *) user_sp_kva, argv[i]);
    }
    pcb->user_sp = user_sp_kva - user_sp_kva_uva_offset;
    pcb->user_sp = ROUNDDOWN(pcb->user_sp, 16);  // alignment to 128 bit = 16 B
    /*--------------------------------KERNEL STACK-------------------------------------*/
    /* initialization of registers on kernel stack*/
    pcb->kernel_sp = pcb->kernel_sp - sizeof(regs_context_t) - sizeof(switchto_context_t);
    regs_context_t *pt_regs = (regs_context_t *) (pcb->kernel_sp + sizeof(switchto_context_t));
    for (int i = 0; i < 32; i++) {
        if (i == 1) // ra
            pt_regs->regs[i] = pcb->entry_point;
        else if (i == 2) // sp
            pt_regs->regs[i] = pcb->user_sp;
        else if (i == 4) // tp
            pt_regs->regs[i] = (reg_t) pcb;
        else if (i == 10) // a0
            pt_regs->regs[i] = argc;
        else if (i == 11) // a1
            pt_regs->regs[i] = argv_base;
        else
            pt_regs->regs[i] = 0;
    }
    pt_regs->sstatus = SR_SPIE | SR_SUM;           // set spp = 0, spie = 1, sie = 0, SUM = 1
    pt_regs->sepc = pcb->entry_point;            // entry 
    /* set sp to simulate just returning from switch_to */
    switchto_context_t *pt_switchto = (switchto_context_t *) (pcb->kernel_sp);
    for (int i = 0; i < 14; i++) {
        if (i == 0) { // ra
            pt_switchto->regs[i] = (reg_t) ret_from_exception;
        } else if (i == 1) {   // sp
            pt_switchto->regs[i] = pcb->kernel_sp;
        } else {      // S0 - S11
            pt_switchto->regs[i] = 0;
        }
    }
}
pid_t do_exec(char *name, int argc, char *argv[]) {
    /* =============================get taskid and pcb_id */
    int taskid;
    if ((taskid = taskname_to_taskid(name)) == -1) {
        return -1;  // fail to find valid task
    }
    int pcb_id;
    if ((pcb_id = get_free_pcb()) == -1) {
        return -1; // fail to find free pcb
    }
    /* ============================init pcb */
    setup_process_pcb(&pcb[pcb_id], tasks[taskid]);
    /* ===========================setup process vm  */
    /* ------setup page directory */
    pcb[pcb_id].pgdir = alloc_kernel_page();   // alloc 4KB for user pgdir
    create_pn(pcb[pcb_id].pgdir, pcb[pcb_id].pgdir, (uintptr_t) PGDIR_VA, pcb);

    clear_pgdir(pcb[pcb_id].pgdir);
    share_pgtable(pcb[pcb_id].pgdir, (uintptr_t) PGDIR_VA);// copy kernel pgdir
    /* -----setup text and data segment vm */
    load_task_img(&pcb[pcb_id], tasks[taskid]);
    /* ------setup user stack vm and init kernel stack */
    setup_process_stack(&pcb[pcb_id], argc, argv);

    /* add to readyqueue */
    add_readyqueue(&pcb[pcb_id]);
    do_scheduler();
    return pcb[pcb_id].pid;

}
int do_kill(pid_t pid) {
    // ----------pid to pcb_id
    int i;
    if ((i = pid_to_pcb_id(pid)) == -1) { // fail to find
        return -1;
    }
    // ------------kill itself, then go to exit
    if (&pcb[i] == current_running) {
        do_exit();
        return 1;
    }
    // ----------wake up block queue
    while (pcb[i].block_queue.next != &pcb[i].block_queue) {
        do_unblock(pcb[i].block_queue.next);
    }
    // -----------release lock
    release_process_mutex(pid);
    // ------------recycle mem
    release_process_page(&pcb[i]);
    // -------------recycle pcb
    pcb[i].status = TASK_EXITED;
    remove_pcb_queue(&pcb[i]);
    return 1;
}
void do_exit(void) {
    // wake up wait queue
    while (current_running->block_queue.next != &current_running->block_queue) {
        do_unblock(current_running->block_queue.next);
    }
    // release lock
    release_process_mutex(current_running->pid);
    // ------------recycle mem
    release_process_page(current_running);
    // recycle pcb
    current_running->status = TASK_EXITED;
    do_scheduler();
}
int do_waitpid(pid_t pid) {
    int i = 0;
    for (;i < NUM_MAX_TASK;i++) {
        if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
            break;
        }
    }
    if (i >= NUM_MAX_TASK) {
        return 0;
    }  // fail to find
    if (pcb[i].status != TASK_EXITED) {
        do_block(&current_running->list, &pcb[i].block_queue);
        do_scheduler();
    } else {
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
        int i = 0;
        for (;i < NUM_MAX_TASK;i++) {
            if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
                break;
            }
        }
        pcb[i].cpu_mask = mask;
    } else {// taskset -p mask pid
        int i = 0;
        for (;i < NUM_MAX_TASK;i++) {
            if (pcb[i].pid == pid && pcb[i].status != TASK_EXITED) {
                break;
            }
        }
        if (i >= NUM_MAX_TASK) {
            return -1;
        }
        pcb[i].cpu_mask = mask;
    }
    return 0;
}
void remove_pcb_queue(pcb_t *pcb) {
    if (pcb->list.next == NULL || pcb->list.next == NULL) {
        return;
    }

    list_node_t *pcb_list = &pcb->list;
    list_node_t *next_node = pcb_list->next;
    list_node_t *prev_node = pcb_list->prev;
    pcb_list->next = NULL;
    pcb_list->prev = NULL;
    next_node->prev = prev_node;
    prev_node->next = next_node;
}