#include <sys/syscall.h>
/* args */
#define OFFSET_REG_A0           80
#define OFFSET_REG_A1           88
#define OFFSET_REG_A2           96
#define OFFSET_REG_A3           104
#define OFFSET_REG_A4           112
#define OFFSET_REG_A5           120
#define OFFSET_REG_A6           128
#define OFFSET_REG_A7           136
#define OFFSET_REG_SEPC         264
long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */
    long retval;
    retval = syscall[regs->regs[OFFSET_REG_A7]](regs->regs[OFFSET_REG_A0], regs->regs[OFFSET_REG_A1], \
        regs->regs[OFFSET_REG_A2], regs->regs[OFFSET_REG_A3], regs->regs[OFFSET_REG_A4]);

    regs->regs[OFFSET_REG_A0] = retval;
    regs->regs[OFFSET_REG_SEPC] += 4;           // sepc += 4
    //ret_from_exception();
    return;
}
