#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>



void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    unlock_kernel(&ready_queue_hart_lock);
    unlock_kernel(&screen_buffer_hart_lock);
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    uint64_t hartid = get_current_cpu_id();
    unsigned long hart_mask = (1UL << hartid);
    hart_mask = ~hart_mask;
    send_ipi(&hart_mask);
    return;
}

void lock_kernel(spinlock_t *hart_lock)
{
    /* TODO: P3-TASK3 multicore*/
    while (atomic_swap(1, hart_lock) != 0) {

    }
    return;
}

void unlock_kernel(spinlock_t *hart_lock)
{
    /* TODO: P3-TASK3 multicore*/
    atomic_swap(0, hart_lock);
    return;
}
