#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
    long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long retval;

    register long r_sysno asm("a7") = sysno;
    register long r_a0 asm("a0") = arg0;
    register long r_a1 asm("a1") = arg1;
    register long r_a2 asm("a2") = arg2;
    register long r_a3 asm("a3") = arg3;
    register long r_a4 asm("a4") = arg4;

    asm volatile (
        "ecall\n\t"
        "mv %0, a0\n\t"
        : "=r" (retval)
        : "r" (r_sysno),
        "r" (r_a0),
        "r" (r_a1),
        "r" (r_a2),
        "r" (r_a3),
        "r" (r_a4)
        : "memory" // 被修改的寄存器和内存
        );
    return retval;
}

void sys_yield(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    // call_jmptab(YIELD, 0, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall((long) SYSCALL_YIELD, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    //call_jmptab(MOVE_CURSOR, (long) x, (long) y, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall((long) SYSCALL_CURSOR, (long) x, (long) y, IGNORE, IGNORE, IGNORE);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    // call_jmptab(SCREEN_WRITE, (long) buff, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall((long) SYSCALL_WRITE, (long) buff, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    //call_jmptab(SCREEN_FLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall((long) SYSCALL_REFLUSH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    //call_jmptab(MUTEX_INIT, (long) key, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    long retval = invoke_syscall((long) SYSCALL_LOCK_INIT, (long) key, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    //call_jmptab(MUTEX_ACQ, (long) mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall((long) SYSCALL_LOCK_ACQ, (long) mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    //call_jmptab(MUTEX_RELEASE, (long) mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall((long) SYSCALL_LOCK_RELEASE, (long) mutex_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    long retval = invoke_syscall((long) SYSCALL_GET_TIMEBASE, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    long retval = invoke_syscall((long) SYSCALL_GET_TICK, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall((long) SYSCALL_SLEEP, (long) time, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_set_sche_workload(int remain_length) {
    invoke_syscall((long) SYSCALL_SET_SCHE_WORKLOAD, (long) remain_length, IGNORE, IGNORE, IGNORE, IGNORE);
}
/************************************************************/
#ifdef S_CORE
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec for S_CORE */
}
#else
pid_t  sys_exec(char *name, int argc, char **argv)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exec */
    pid_t retval = (pid_t) invoke_syscall((long) SYSCALL_EXEC, (long) name, (long) argc, (long) argv, IGNORE, IGNORE);
    return retval;
}
#endif

void sys_exit(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_exit */
    invoke_syscall((long) SYSCALL_EXIT, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

int  sys_kill(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_kill */
    int retval = invoke_syscall((long) SYSCALL_KILL, (long) pid, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

int  sys_waitpid(pid_t pid)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_waitpid */
    int retval = invoke_syscall((long) SYSCALL_WAITPID, (long) pid, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}


void sys_ps(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_ps */
    invoke_syscall((long) SYSCALL_PS, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_screen_clear(void) {
    invoke_syscall((long) SYSCALL_CLEAR, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
}
pid_t sys_getpid()
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getpid */
    int retval = invoke_syscall((long) SYSCALL_GETPID, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

int  sys_getchar(void)
{
    /* TODO: [p3-task1] call invoke_syscall to implement sys_getchar */
    int retval = invoke_syscall((long) SYSCALL_READCH, IGNORE, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

int  sys_barrier_init(int key, int goal)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrier_init */
    int retval = invoke_syscall((long) SYSCALL_BARR_INIT, (long) key, (long) goal, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_barrier_wait(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_wait */
    invoke_syscall((long) SYSCALL_BARR_WAIT, (long) bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_barrier_destroy(int bar_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_barrie_destory */
    invoke_syscall((long) SYSCALL_BARR_DESTROY, (long) bar_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_condition_init(int key)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_init */
    int retval = invoke_syscall((long) SYSCALL_COND_INIT, (long) key, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_condition_wait(int cond_idx, int mutex_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_wait */
    invoke_syscall((long) SYSCALL_COND_WAIT, (long) cond_idx, (long) mutex_idx, IGNORE, IGNORE, IGNORE);
}

void sys_condition_signal(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_signal */
    invoke_syscall((long) SYSCALL_COND_SIGNAL, (long) cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_broadcast(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_broadcast */
    invoke_syscall((long) SYSCALL_COND_BROADCAST, (long) cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_condition_destroy(int cond_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_condition_destroy */
    invoke_syscall((long) SYSCALL_COND_DESTROY, (long) cond_idx, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_semaphore_init(int key, int init)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_init */
}

void sys_semaphore_up(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_up */
}

void sys_semaphore_down(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_down */
}

void sys_semaphore_destroy(int sema_idx)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_semaphore_destroy */
}

int sys_mbox_open(char *name)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_open */
    int retval = invoke_syscall((long) SYSCALL_MBOX_OPEN, (long) name, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_mbox_close(int mbox_id)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_close */
    invoke_syscall((long) SYSCALL_MBOX_CLOSE, (long) mbox_id, IGNORE, IGNORE, IGNORE, IGNORE);
}

int sys_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_send */
    int retval = invoke_syscall((long) SYSCALL_MBOX_SEND, (long) mbox_idx, (long) msg, (long) msg_length, IGNORE, IGNORE);
    return retval;
}

int sys_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    /* TODO: [p3-task2] call invoke_syscall to implement sys_mbox_recv */
    int retval = invoke_syscall((long) SYSCALL_MBOX_RECV, (long) mbox_idx, (long) msg, (long) msg_length, IGNORE, IGNORE);
    return retval;
}
int sys_taskset(char *name, pid_t pid, uint64_t mask, int mod) {
    int retval = invoke_syscall((long) SYSCALL_TASKSET, (long) name, (long) pid, (long) mask, (long) mod, IGNORE);
    return retval;
}

void *sys_shmpageget(int key)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpageget */
    void *retval = invoke_syscall((long) SYSCALL_SHM_GET, (long) key, IGNORE, IGNORE, IGNORE, IGNORE);
    return retval;
}

void sys_shmpagedt(void *addr)
{
    /* TODO: [p4-task4] call invoke_syscall to implement sys_shmpagedt */
    invoke_syscall((long) SYSCALL_SHM_DT, (long) addr, IGNORE, IGNORE, IGNORE, IGNORE);
}

void sys_pthread_create(pthread_t thread, void (*start_routine)(void *), void *arg, uint64_t exit_funct)
{
    invoke_syscall((long) SYSCALL_PTHREAD_CREATE, (long) thread, (long) start_routine, (long) arg, (long) exit_funct, IGNORE);
}
void sys_pthread_join(pthread_t thread)
{
    invoke_syscall((long) SYSCALL_PTHREAD_JOIN, (long) thread, IGNORE, IGNORE, IGNORE, IGNORE);
}


int sys_mprotect(void *addr, size_t len, int prot) {
    int retval = invoke_syscall((long) SYSCALL_MPTOTECT, (long) addr, (long) len, (long) prot, IGNORE, IGNORE);
    return retval;
}

void sys_getbrk(uint64_t bss_end)
{
    invoke_syscall((long) SYSCALL_GET_BRK, (long) bss_end, IGNORE, IGNORE, IGNORE, IGNORE);
}

int brk(void *addr)
{
    invoke_syscall((long) SYSCALL_BRK, (long) addr, IGNORE, IGNORE, IGNORE, IGNORE);
}

void *sbrk(intptr_t increment)
{
    invoke_syscall((long) SYSCALL_SBRK, (long) increment, IGNORE, IGNORE, IGNORE, IGNORE);
}


int sys_net_send(void *txpacket, int length)
{
    /* TODO: [p5-task1] call invoke_syscall to implement sys_net_send */
    int retval = invoke_syscall((long) SYSCALL_NET_SEND, (long) txpacket, (long) length, IGNORE, IGNORE, IGNORE);
    return retval;
}

int sys_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    /* TODO: [p5-task2] call invoke_syscall to implement sys_net_recv */
    int retval = invoke_syscall((long) SYSCALL_NET_RECV, (long) rxbuffer, (long) pkt_num, (long) pkt_lens, IGNORE, IGNORE);
    return retval;
}
/************************************************************/