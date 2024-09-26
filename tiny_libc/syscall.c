#include <syscall.h>
#include <stdint.h>
#include <kernel.h>
#include <unistd.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
    long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    asm volatile("nop");

    return 0;
}

void sys_yield(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_yield */
    // call_jmptab(YIELD, 0, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall((long) SYSCALL_YIELD, 0, 0, 0, 0, 0);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_move_cursor */
    //call_jmptab(MOVE_CURSOR, (long) x, (long) y, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall((long) SYSCALL_CURSOR, (long) x, (long) y, 0, 0, 0);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_write */
    // call_jmptab(SCREEN_WRITE, (long) buff, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall((long) SYSCALL_WRITE, (long) buff, 0, 0, 0, 0);
}

void sys_reflush(void)
{
    /* TODO: [p2-task1] call call_jmptab to implement sys_reflush */
    //call_jmptab(SCREEN_FLUSH, 0, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall((long) SYSCALL_REFLUSH, 0, 0, 0, 0, 0);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_init */
    //call_jmptab(MUTEX_INIT, (long) key, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    invoke_syscall((long) SYSCALL_LOCK_INIT, (long) key, 0, 0, 0, 0);
    return 0;
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_acquire */
    //call_jmptab(MUTEX_ACQ, (long) mutex_idx, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall((long) SYSCALL_LOCK_ACQ, (long) mutex_idx, 0, 0, 0, 0);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task2] call call_jmptab to implement sys_mutex_release */
    //call_jmptab(MUTEX_RELEASE, (long) mutex_idx, 0, 0, 0, 0);
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall((long) SYSCALL_LOCK_RELEASE, (long) mutex_idx, 0, 0, 0, 0);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    //invoke_syscall((long) SYSCALL_CURSOR, (long) x, (long) y, 0, 0, 0);
    return 0;
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    //invoke_syscall((long) SYSCALL_CURSOR, (long) x, (long) y, 0, 0, 0);
    return 0;
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    //invoke_syscall((long) SYSCALL_CURSOR, (long) x, (long) y, 0, 0, 0);
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/