#ifndef SMP_H
#define SMP_H

#define NR_CPUS 2
typedef volatile uint32_t hart_spinlock_t;
hart_spinlock_t ready_queue_hart_lock;
hart_spinlock_t screen_buffer_hart_lock;
hart_spinlock_t mutex_hart_lock;
hart_spinlock_t barrier_hart_lock;
hart_spinlock_t condition_hart_lock;
hart_spinlock_t mailbox_hart_lock;
hart_spinlock_t pcb_pid_hart_lock;
hart_spinlock_t sleep_queue_lock;

#define slave_hart_lock_loc 0xffffffc0502001f4

extern void smp_init();
extern void wakeup_other_hart();
extern uint64_t get_current_cpu_id();
extern void lock_kernel(hart_spinlock_t *hart_lock);
extern void unlock_kernel(hart_spinlock_t *hart_lock);

#endif /* SMP_H */