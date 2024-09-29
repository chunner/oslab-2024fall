#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>

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
    pcb_t *prepcb = current_running;
    // current_running =(pcb_t *) ((char *)current_running ->list.next - sizeof(reg_t) * 2);
    current_running = LIST_PCB(ready_queue.next);
    ready_queue.next = ready_queue.next->next;       // delete current_running from ready_queue
    if (prepcb->status == TASK_READY)
        add_readyqueue(prepcb);
    // TODO: [p2-task1] switch_to current_running
    if (current_running != NULL)
        // switch_to(prepcb, current_running);
        return;         //  return -> handle_syscall -> interrupt_helper -> ret_from_complete
}                       // or return -> main -> ret_from_complete

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->wakeup_time = sleep_time * get_time_base() + get_ticks();
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
    pcb->status = TASK_READY;
}
