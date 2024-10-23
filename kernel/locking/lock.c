#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <atomic.h>
#include <os/string.h>

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
        barrier[i].status = BAR_INACTIVE;
        barrier[i].block_queue.next = &barrier[i].block_queue;
        barrier[i].block_queue.prev = &barrier[i].block_queue;
    }
}
int do_barrier_init(int key, int goal) {
    int bar_idx = 0;
    for (;bar_idx < BARRIER_NUM;bar_idx++) {
        if (barrier[bar_idx].status == BAR_INACTIVE) {
            break;
        }
    }
    if (bar_idx >= BARRIER_NUM)  return -1; // barrier are run out
    barrier[bar_idx].status = BAR_ACTIVE;
    barrier[bar_idx].goal = goal;
    barrier[bar_idx].key = key;
    barrier[bar_idx].counter = 0;
    barrier[bar_idx].block_queue.next = &barrier[bar_idx].block_queue;
    barrier[bar_idx].block_queue.prev = &barrier[bar_idx].block_queue;
    return bar_idx;
}
void do_barrier_wait(int bar_idx) {
    barrier[bar_idx].counter++;
    if (barrier[bar_idx].counter >= barrier[bar_idx].goal) {
        while (barrier[bar_idx].block_queue.next != &barrier[bar_idx].block_queue) {    // wake up block queue
            do_unblock(barrier[bar_idx].block_queue.next);
        }
        barrier[bar_idx].counter = 0;
        return;
    } else {
        do_block(&current_running->list, &barrier[bar_idx].block_queue);
        do_scheduler();
    }
}
void do_barrier_destroy(int bar_idx) {
    while (barrier[bar_idx].block_queue.next != &barrier[bar_idx].block_queue) {    // wake up block queue
        do_unblock(barrier[bar_idx].block_queue.next);
    }
    barrier[bar_idx].status = BAR_INACTIVE;
}

/* ---------------------------condition-----------------------------------------*/
condition_t condition[CONDITION_NUM];
void init_conditions(void) {
    for (int i = 0;i < CONDITION_NUM;i++) {
        condition[i].block_queue.next = &condition[i].block_queue;
        condition[i].block_queue.prev = &condition[i].block_queue;
        condition[i].status = COND_INACTIVE;
    }
}
int do_condition_init(int key) {
    int cond_idx = 0;
    for (;cond_idx < CONDITION_NUM;cond_idx++) {
        if (condition[cond_idx].status == COND_INACTIVE) {
            break;
        }
    }
    if (cond_idx >= CONDITION_NUM)    return -1; // condition has run out
    condition[cond_idx].key = key;
    condition[cond_idx].status = COND_ACTIVE;
    condition[cond_idx].block_queue.next = &condition[cond_idx].block_queue;
    condition[cond_idx].block_queue.prev = &condition[cond_idx].block_queue;
    return cond_idx;
}
void do_condition_wait(int cond_idx, int mutex_idx) {
    do_block(&current_running->list, &condition[cond_idx].block_queue);
    do_mutex_lock_release(mutex_idx);
    do_scheduler();
    do_mutex_lock_acquire(mutex_idx);
}
void do_condition_signal(int cond_idx) {
    if (condition[cond_idx].block_queue.next != &condition[cond_idx].block_queue) {
        do_unblock(condition[cond_idx].block_queue.next);
    }
}
void do_condition_broadcast(int cond_idx) {
    while (condition[cond_idx].block_queue.next != &condition[cond_idx].block_queue) {
        do_unblock(condition[cond_idx].block_queue.next);
    }
}
void do_condition_destroy(int cond_idx) {
    do_condition_broadcast(cond_idx);
    condition[cond_idx].status = COND_INACTIVE;
}
/*----------------------------------------mail box-----------------------------------------*/
mailbox_t mailbox[MBOX_NUM];
void init_mbox() {
    for (int i = 0;i < MBOX_NUM;i++) {
        mailbox[i].status = MBOX_INACTIVE;
        mailbox[i].open = MBOX_CLOSE;
        mailbox[i].cite_num = 0;
        // init block queue
        mailbox[i].empty_queue.next = &mailbox[i].empty_queue;
        mailbox[i].empty_queue.prev = &mailbox[i].empty_queue;
        mailbox[i].full_queue.next = &mailbox[i].full_queue;
        mailbox[i].full_queue.prev = &mailbox[i].full_queue;
    }
}
int do_mbox_open(char *name) {
    int mbox_idx = 0;
    // try to search a mailbox with the same name
    for (;mbox_idx < MBOX_NUM;mbox_idx++) {
        if (mailbox[mbox_idx].status == MBOX_ACTIVE && strcmp(mailbox[mbox_idx].name, name) == 0) {
            break;
        }
    }
    if (mbox_idx >= MBOX_NUM) { // fail to search
        mbox_idx = 0;
        for (;mbox_idx < MBOX_NUM;mbox_idx++) {
            if (mailbox[mbox_idx].status == MBOX_INACTIVE) {
                break;
            }
        }
        if (mbox_idx >= MBOX_NUM)   // mailbox has run out
            return -1;
        // init the mailbox
        strcpy(mailbox[mbox_idx].name, name);
        mailbox[mbox_idx].buffer_idx = 0;
        mailbox[mbox_idx].cite_num = 0;
        mailbox[mbox_idx].empty_queue.next = &mailbox[mbox_idx].empty_queue;
        mailbox[mbox_idx].empty_queue.prev = &mailbox[mbox_idx].empty_queue;
        mailbox[mbox_idx].full_queue.next = &mailbox[mbox_idx].full_queue;
        mailbox[mbox_idx].full_queue.prev = &mailbox[mbox_idx].full_queue;
    }
    mailbox[mbox_idx].cite_num++;
    mailbox[mbox_idx].open = MBOX_OPEN;
    return mbox_idx;
}
void do_mbox_close(int mbox_idx) {
    mailbox[mbox_idx].cite_num--;
    mailbox[mbox_idx].open = MBOX_CLOSE;
    if (mailbox[mbox_idx].cite_num <= 0) {
        mailbox[mbox_idx].status = MBOX_INACTIVE;
    }
}
int do_mbox_send(int mbox_idx, void *msg, int msg_length) {
    while (1) {
        if (mailbox[mbox_idx].buffer_idx + msg_length <= MAX_MBOX_LENGTH) {
            int start = mailbox[mbox_idx].buffer_idx;
            strncpy(&mailbox[mbox_idx].buffer[start], (char *) msg, msg_length);
            mailbox[mbox_idx].buffer_idx += msg_length;
            // wake up all reciver
            while (mailbox[mbox_idx].empty_queue.next != &mailbox[mbox_idx].empty_queue) {
                do_unblock(mailbox[mbox_idx].empty_queue.next);
            }
            return 0;
        } else {    // mailbox is full
            do_block(&current_running->list, &mailbox[mbox_idx].full_queue);
            do_scheduler();
        }
    }
}
int do_mbox_recv(int mbox_idx, void *msg, int msg_length) {
    while (1) {
        if (mailbox[mbox_idx].buffer_idx - msg_length < 0) {
            int start = mailbox[mbox_idx].buffer_idx - msg_length;
            strncpy((char *) msg, &mailbox[mbox_idx].buffer[start], msg_length);
            mailbox[mbox_idx].buffer_idx = start;
            // wake up all sender
            while (mailbox[mbox_idx].full_queue.next != &mailbox[mbox_idx].full_queue) {
                do_unblock(mailbox[mbox_idx].full_queue.next);
            }
            return 0;
        } else {
            do_block(&current_running->list, &mailbox[mbox_idx].empty_queue);
            do_scheduler();
        }
    }
}