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
    for (;i <= last_lockid; i++) {
        if (mlocks[i].key == key) {
            break;
        }
    }
    if (last_lockid >= LOCK_NUM) return -1;
    mlocks[i].key = key;
    return i;
}


void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    //spin_lock_acquire(&mlocks[mlock_idx].lock);
    while (1) {
        if (mlocks[mlock_idx].lock.status == UNLOCKED) {
            mlocks[mlock_idx].lock.status = LOCKED;
            return;
        } else {
            do_block(&current_running->list, &mlocks[mlock_idx].block_queue);
            do_scheduler();
            ret_from_exception();
        }
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mlocks[mlock_idx].lock.status = UNLOCKED;
    while (mlocks[mlock_idx].block_queue.next != &mlocks[mlock_idx].block_queue) {
        do_unblock(mlocks[mlock_idx].block_queue.next);
    }
    //do_scheduler();
}
