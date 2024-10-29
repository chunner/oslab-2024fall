#include <os/list.h>
#include <os/sched.h>
#include <type.h>
#include <os/smp.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    lock_kernel(&block_queue_lock);
    list_node_t *p = sleep_queue.next;
    list_node_t *pnext;
    while (p != &sleep_queue) {
        pnext = p->next;
        pcb_t *pcb = LIST_PCB(p);
        if (pcb->wakeup_time <= get_timer()) {
            do_unblock(p);
        }
        p = pnext;
    }
    unlock_kernel(&block_queue_lock);
}