#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

typedef volatile uint32_t spinlock_t;
spinlock_t hart_lock = 0;


void hart_lock_acquire() {
    while (atomic_cmpxchg(0, 1, (ptr_t) &hart_lock) != 0) {
    }
}

void hart_lock_release() {
    hart_lock = 0;
}

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
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

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
}
