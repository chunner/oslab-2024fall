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

#define UserStackPage 10
#define KernelStackPage 10


int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

extern void ret_from_exception();

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
    regs_context_t *pt_regs =
        (regs_context_t *) (kernel_stack - sizeof(regs_context_t));
    for (int i = 0; i < 32; i++) {
        if (i == 1) // ra
            pt_regs->regs[i] = entry_point;
        else if (i == 2) // sp
            pt_regs->regs[i] = user_stack;
        else
            pt_regs->regs[i] = 0;
    }
    pt_regs->sstatus = pt_regs->sstatus & (~SR_SPP) | SR_SPIE & (~SR_SIE);           // set spp = 0, spie = 1, sid = 0
    pt_regs->sepc = entry_point;            // entry pointer of Interrupt handling function

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *) ((ptr_t) pt_regs - sizeof(switchto_context_t));
    for (int i = 0; i < 14; i++) {
        if (i == 0) { // ra
            pt_switchto->regs[i] = entry_point;
        } else if (i == 1) {   // sp
            pt_switchto->regs[i] = user_stack;
        } else {      // S0 - S11
            pt_switchto->regs[i] = 0;
        }
    }
    pcb->kernel_sp = kernel_stack - sizeof(regs_context_t) - sizeof(switchto_context_t);
}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0;i < tasknum; i++) {
        pcb[i].kernel_sp = allocKernelPage(KernelStackPage) + KernelStackPage * PAGE_SIZE;
        pcb[i].user_sp = allocUserPage(UserStackPage) + UserStackPage * PAGE_SIZE;
        pcb[i].pid = i + 2;  // user pid start from 2
        pcb[i].status = TASK_READY;
        pcb[i].entry_point = tasks[i].entry;
        init_pcb_stack(pcb[i].kernel_sp, pcb[i].user_sp, pcb[i].entry_point, &pcb[i]);
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running = &pid0_pcb;
    current_running->list.next = &pcb[0].list;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    init_exception();
    setup_exception();
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






}
/************************************************************/

void add_readyqueue(pcb_t *pcb)        // add the tail of ready_queue
{
    if (!pcb)   return;  // pcb = NULL
    list_node_t *p = &ready_queue;
    while (p->next != &ready_queue) {
        p = p->next;
    }
    pcb->list.next = p->next;   // &ready_queue
    p->next = &pcb->list;
}
static pcb_t *taskname2pcb(char taskname[]) {
    int i = 0;
    for (; i < tasknum; i++) {
        if (strcmp(taskname, tasks[i].taskname) == 0) {
            break;
        }
    }
    if (i == tasknum) {             // task name match failed
        port_write("taskname: \"");
        port_write(taskname);
        port_write("\" does not exit\n\r");
        return NULL;
    } else {
        return &pcb[i];
    }
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's

    // load all tasks from sd to mem
    for (int i = 0; i < tasknum; i++) {
        load_task_img(tasks[i].taskname);
    }
    /* input the command */
    //manage_input();
    // task1
    add_readyqueue(taskname2pcb("print1"));
    add_readyqueue(taskname2pcb("fly"));
    add_readyqueue(taskname2pcb("print2"));
    add_readyqueue(taskname2pcb("lock1"));
    add_readyqueue(taskname2pcb("lock2"));
    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do non-preemptive scheduling, it's used to surrender control
        do_scheduler();

        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // enable_preempt();
        // asm volatile("wfi");
    }

    return 0;
}
