#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>

typedef volatile uint32_t spinlock_t;
spinlock_t hart_lock;


void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    unlock_kernel();
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
    while (atomic_swap(1, &hart_lock) != 0) {

    }
    //printl("ok\n");
    // asm volatile(
    //     "1: li t0, 1\n"
    //     "   amoswap.w.aq t1, t0, (%0)\n"
    //     "   bnez t1, 1b\n"
    //     :           // no output
    // : "r" (&hart_lock)  // input
    //     : "t0", "t1"    // changed
    //     );
    return;
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    //printl("ulock\n");
    // hart_lock = 0;
    atomic_swap(0, &hart_lock);
    // asm volatile(
    //     "   li t0, 0\n"
    //     "   amoswap.w.rl x0, t0, (%0)\n"
    //     :
    // : "r" (&hart_lock)
    //     : "t0"
    //     );
    return;
}
