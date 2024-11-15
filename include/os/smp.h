#ifndef SMP_H
#define SMP_H

#define NR_CPUS 2
typedef volatile uint32_t hart_spinlock_t;

hart_spinlock_t whole_kernel_lock;

#define slave_hart_lock_loc 0xffffffc0502001f4

extern void smp_init();
extern void wakeup_other_hart();
extern uint64_t get_current_cpu_id();
extern void lock_kernel(hart_spinlock_t *hart_lock);
extern void unlock_kernel(hart_spinlock_t *hart_lock);

#endif /* SMP_H */