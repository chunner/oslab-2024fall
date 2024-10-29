#ifndef SMP_H
#define SMP_H

#define NR_CPUS 2
extern void smp_init();
extern void wakeup_other_hart();
extern uint64_t get_current_cpu_id();
extern void lock_kernel(spinlock_t *hart_lock);
extern void unlock_kernel(spinlock_t *hart_lock);

typedef volatile uint32_t spinlock_t;
spinlock_t ready_queue_hart_lock;
spinlock_t screen_buffer_hart_lock;
spinlock_t mutex_hart_lock;
spinlock_t barrier_hart_lock;

#endif /* SMP_H */