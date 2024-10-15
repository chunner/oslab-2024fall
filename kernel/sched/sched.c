#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

pcb_t *active_pcb[NUM_MAX_TASK];
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
        if (current_running->pid == 0) {        // the first time to exec
            active_pcb[process_id] = current_running;
            current_running->pid = process_id++;
        }
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
void process_show() {
    printk("[Process Table]\n");
    for (int i = 0;i < process_id - 1;i++) {
        printk("[%d] PID : %d   STATUS : ", i, i + 1);
        switch (active_pcb[i + 1]->status)
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