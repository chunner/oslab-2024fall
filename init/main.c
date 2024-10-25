#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#define VERSION_BUF 50
#define nmultask 4
#define COMMAND_LEN 50


#define task_info_new_loc 0x58000010  // user's sp + 0x10
#define kernel          0x50201000
#define tasknum_loc     0x502001f6



int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

extern void ret_from_exception();
extern uint64_t get_current_cpu_id();

// Task info array
task_info_t tasks[TASK_MAXNUM];
short tasknum;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}
static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR] = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ] = (long (*)())sd_read;
    jmptab[SD_WRITE] = (long (*)())sd_write;
    jmptab[QEMU_LOGGING] = (long (*)())qemu_logging;
    jmptab[SET_TIMER] = (long (*)())set_timer;
    jmptab[READ_FDT] = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR] = (long (*)())screen_move_cursor;
    jmptab[PRINT] = (long (*)())printk;
    jmptab[YIELD] = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT] = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ] = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE] = (long (*)())do_mutex_lock_release;
    jmptab[SCREEN_FLUSH] = (long (*)())screen_reflush;   // reflush screen buffer
    jmptab[SCREEN_WRITE] = (long (*)())screen_write;   // screen write string
    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // NOTE: You need to get some related arguments from bootblock first
    /* get taskinfo from memory */
    short *tasknum_mem = (short *) tasknum_loc;
    tasknum = *tasknum_mem;
    task_info_t *taskinfo_mem = (task_info_t *) (task_info_new_loc);
    for (int i = 0; i < tasknum; i++) {
        tasks[i].sector_num = taskinfo_mem[i].sector_num;
        tasks[i].firstsector = taskinfo_mem[i].firstsector;
        tasks[i].offset = taskinfo_mem[i].offset;
        tasks[i].entry = taskinfo_mem[i].entry;
        strcpy(tasks[i].taskname, taskinfo_mem[i].taskname);
    }
}

int taskname_to_taskid(char taskname[]) {
    int i = 0;
    for (; i < tasknum; i++) {
        if (strcmp(taskname, tasks[i].taskname) == 0) {
            break;
        }
    }
    if (i == tasknum) {             // task name match failed
        // port_write("taskname: \"");
        // port_write(taskname);
        // port_write("\" does not exit\n\r");
        return -1;
    } else {
        return i;
    }
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    pcb->kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
    regs_context_t *pt_regs =
        (regs_context_t *) (kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++) {
        if (i == 1) // ra
            pt_regs->regs[i] = entry_point;
        else if (i == 2) // sp
            pt_regs->regs[i] = user_stack;
        else if (i == 4) // tp
            pt_regs->regs[i] = (reg_t) pcb;
        else
            pt_regs->regs[i] = 0;
    }
    pt_regs->sstatus = ((0UL & (~SR_SPP)) & (~SR_SIE)) | SR_SPIE;           // set spp = 0, spie = 1, sie = 0
    pt_regs->sepc = entry_point;            // entry 
    pt_regs->scause = 0UL | EXC_SYSCALL;    // IRQ 
    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *) ((ptr_t) pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++) {
        if (i == 0) { // ra
            pt_switchto->regs[i] = (reg_t) ret_from_exception;
        } else if (i == 1) {   // sp
            pt_switchto->regs[i] = pcb->kernel_sp;
        } else {      // S0 - S11
            pt_switchto->regs[i] = 0;
        }
    }
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0;i < NUM_MAX_TASK; i++) {
        pcb[i].pcb_status = PCB_INACTIVE;
    }
    // pcb[0] is shell
    pcb[0].kernel_sp = allocKernelPage(KernelStackPage) + KernelStackPage * PAGE_SIZE;
    pcb[0].user_sp = allocUserPage(UserStackPage) + UserStackPage * PAGE_SIZE;
    pcb[0].pid = 2;
    pcb[0].status = TASK_BLOCKED;
    pcb[0].entry_point = tasks[taskname_to_taskid("shell")].entry;
    pcb[0].remain_length = 0;
    pcb[0].block_queue.next = &pcb[0].block_queue;
    pcb[0].block_queue.prev = &pcb[0].block_queue;
    pcb[0].pcb_status = PCB_ACTIVE;
    strcpy(pcb[0].taskname, "shell");
    init_pcb_stack(pcb[0].kernel_sp, pcb[0].user_sp, pcb[0].entry_point, &pcb[0]);

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_SET_SCHE_WORKLOAD] = (long (*)())set_sche_workload;
    syscall[SYSCALL_GETCH] = (long (*)())bios_getchar;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
}
/************************************************************/

int main(void)
{
    uint64_t mhartid;
    if ((mhartid = get_current_cpu_id()) != 0) { // if not master hart
        printk("mhart id : %s start work \n", mhartid);
        current_running = &pid1_pcb;
        bios_set_timer(time_base * 5 + get_ticks());
        while (1)
        {
            enable_preempt();
        }
    }
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);        // time_base = 10000000

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init barriers, condition, mailbox
    init_barriers();
    init_conditions();
    init_mbox();

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    printk("mhart id : %s start work \n", mhartid);

    // load all tasks from sd to mem
    for (int i = 0; i < tasknum; i++) {
        load_task_img(tasks[i].taskname);
    }

    add_readyqueue(&pcb[0]);          // start shell

    wakeup_other_hart();
    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    bios_set_timer(time_base * 5 + get_ticks());

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        // do_scheduler();
        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
