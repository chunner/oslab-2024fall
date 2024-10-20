#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
int last_lockid = 0;
void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++) {
        mlocks[i].block_queue.next = &mlocks[i].block_queue;
        mlocks[i].block_queue.prev = &mlocks[i].block_queue;
        mlocks[i].pid = 0;
        spin_lock_init(&mlocks[i].lock);
    }
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return 0;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int i = 0;
    for (;i < last_lockid; i++) {
        if (mlocks[i].key == key) {
            break;
        }
    }
    if (i == last_lockid) { // do not exit the same key
        if (i >= LOCK_NUM)   return -1; // mlocks are run out
        mlocks[last_lockid++].key = key;
    }
    return i;
}


void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    //spin_lock_acquire(&mlocks[mlock_idx].lock);
    while (1) {
        if (mlocks[mlock_idx].lock.status == UNLOCKED) {
            mlocks[mlock_idx].lock.status = LOCKED;
            mlocks[mlock_idx].pid = do_getpid();
            return;
        } else {
            do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
            do_scheduler();
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mlocks[mlock_idx].lock.status = UNLOCKED;
    mlocks[mlock_idx].pid = 0;
    while (mlocks[mlock_idx].block_queue.next != &mlocks[mlock_idx].block_queue) {
        do_unblock(mlocks[mlock_idx].block_queue.next);
    }
}
void check_lock(pid_t pid) {
    for (int i = 0;i < last_lockid;i++) {
        if (mlocks[i].lock.status == LOCKED && mlocks[i].pid == pid) {
            do_mutex_lock_release(i);
        }
    }
}
/*--------------------------------barrier----------------------------------------------------------------*/
barrier_t barrier[BARRIER_NUM];
void init_barriers(void) {
    for (int i = 0;i < BARRIER_NUM;i++) {
        barrier[i].status = INACTIVE;
        barrier[i].block_queue.next = &barrier[i].block_queue;
        barrier[i].block_queue.prev = &barrier[i].block_queue;
    }
}
int do_barrier_init(int key, int goal) {
    int bar_idx = 0;
    for (;bar_idx < BARRIER_NUM;bar_idx++) {
        if (barrier[bar_idx].status == INACTIVE) {
            break;
        }
    }
    if (bar_idx >= BARRIER_NUM)  return -1; // barrier are run out
    barrier[bar_idx].status = ACTIVE;
    barrier[bar_idx].goal = goal;
    barrier[bar_idx].key = key;
    barrier[bar_idx].counter = 0;
    barrier[bar_idx].block_queue.next = &barrier[bar_idx].block_queue;
    barrier[bar_idx].block_queue.prev = &barrier[bar_idx].block_queue;
    return bar_idx;
}
void do_barrier_wait(int bar_idx) {
    barrier[bar_idx].counter++;
    while (1) {
        if (barrier[bar_idx].counter >= barrier[bar_idx].goal) {
            while (barrier[bar_idx].block_queue.next != &barrier[bar_idx].block_queue) {    // wake up block queue
                do_unblock(barrier[bar_idx].block_queue.next);
            }
            return;
        } else {
            do_block(&current_running->list, &barrier[bar_idx].block_queue);
            do_scheduler();
        }
    }
}
void do_barrier_destroy(int bar_idx) {
    while (barrier[bar_idx].block_queue.next != &barrier[bar_idx].block_queue) {    // wake up block queue
        do_unblock(barrier[bar_idx].block_queue.next);
    }
    barrier[bar_idx].status = INACTIVE;
}