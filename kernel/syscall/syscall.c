#include <sys/syscall.h>
/* args */
#define NO_REG_A0           10
#define NO_REG_A1           11
#define NO_REG_A2           12
#define NO_REG_A3           13
#define NO_REG_A4           14
#define NO_REG_A5           15
#define NO_REG_A6           16
#define NO_REG_A7           17
#define NO_REG_SEPC         33
long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    long retval;

    retval = syscall[regs->regs[NO_REG_A7]](regs->regs[NO_REG_A0], regs->regs[NO_REG_A1], \
        regs->regs[NO_REG_A2], regs->regs[NO_REG_A3], regs->regs[NO_REG_A4]);
    regs->regs[NO_REG_SEPC] += 4;           // sepc += 4
    regs->regs[NO_REG_A0] = retval;

    return;
}
