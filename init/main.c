/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *         The kernel's entry, where most of the initialization work is done.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

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
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <screen.h>
#include <e1000.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>
#include <pgtable.h>
#include <os/net.h>

#define VERSION_BUF 50

#define tasknum_loc     0xffffffc0502001f6

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

extern void ret_from_exception();
extern uint64_t get_current_cpu_id();

// Task info array
task_info_t tasks[TASK_MAXNUM] __attribute__((section(".data"))) = { 0 };   // Explicitly specify the segment where the tasks array is stored
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

}

static void init_task_info(void)
{
    // NOTE: You need to get some related arguments from bootblock first
    tasknum = *(short *) tasknum_loc;
}

/************************************************************/

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    for (int i = 0;i < NUM_MAX_TASK; i++) {
        pcb[i].status = TASK_EXITED;
    }

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    current_running_0 = &master_pid0_pcb;
    current_running = &master_pid0_pcb;

    current_running_1 = &slave_pid0_pcb;
}

static void init_syscall(void)
{
    // initialize system call table.
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
    syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
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
    syscall[SYSCALL_TASKSET] = (long (*)())do_taskset;
    syscall[SYSCALL_PTHREAD_CREATE] = (long (*)()) do_pthread_create;
    syscall[SYSCALL_PTHREAD_JOIN] = (long(*)()) do_pthread_join;
    syscall[SYSCALL_SHM_GET] = (long(*)()) shm_page_get;
    syscall[SYSCALL_SHM_DT] = (long(*)()) shm_page_dt;
    syscall[SYSCALL_MPTOTECT] = (long(*)()) do_mprotect;
    syscall[SYSCALL_GET_BRK] = (long(*)()) do_getbrk;
    syscall[SYSCALL_BRK] = (long(*)()) do_brk;
    syscall[SYSCALL_SBRK] = (long(*)()) do_sbrk;
    syscall[SYSCALL_NET_SEND] = (long(*)()) do_net_send;
    syscall[SYSCALL_NET_RECV] = (long(*)()) do_net_recv;
}
/************************************************************/

/*
 * Once a CPU core calls this function,
 * it will stop executing!
 */
static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

static void cancel_temp_pgdir() {          // Cancel temporary mapping 0x5000_0000 to 0x5100_0000, in the same secondary pgdir
    PTE *lv2_pgdir = (PTE *) PGDIR_VA;
    uint64_t va = 0x50000000lu;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    clear_pgdir(pa2kva(get_pa(lv2_pgdir[vpn2])));     // clear whole first level page dir
    lv2_pgdir[vpn2] = 0ul;         // Second level page table entry
}

int main(void)
{
    short *slave_hart_lock = (short *) slave_hart_lock_loc;
    uint64_t mhartid;
    if ((mhartid = get_current_cpu_id()) != 0) { // if not master hart
        *slave_hart_lock = 0;
        // move the slave hart cursor
        current_running_1->cursor_y = current_running_0->cursor_y + 1;
        current_running = current_running_1;
        // set stvec,  sstatus: enable interrupts globally
        setup_exception();

        printk("> [INIT] CPU #%u has entered kernel with VM!\n", (unsigned int) get_current_cpu_id());
        //kernel_brake();
        bios_set_timer(time_base / 100 + get_ticks());
        while (1)
        {
            enable_preempt();
            asm volatile("wfi");
        }
    }
    init_mem_manager();
    init_shmpage();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init smp
    smp_init();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read Flatten Device Tree (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);        // time_base = 10000000

    e1000 = (volatile uint8_t *) bios_read_fdt(ETHERNET_ADDR);
    uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
    uint32_t nr_irqs = (uint32_t) bios_read_fdt(NR_IRQS);
    printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

    // IOremap
    plic_addr = (uintptr_t) ioremap((uint64_t) plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
    e1000 = (uint8_t *) ioremap((uint64_t) e1000, 8 * NORMAL_PAGE_SIZE);
    printk("> [INIT] IOremap initialization succeeded.\n");
    PTE *pte = uva2pte((uintptr_t) e1000, (PTE *) PGDIR_VA);
    PTE *pte1 = uva2pte((uintptr_t) (e1000 + 0xc0), (PTE *) PGDIR_VA);
    uint64_t debug = get_pa(*pte);
    uint64_t debug1 = get_pa(*pte1);


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

    // TODO: [p5-task4] Init plic
    // plic_init(plic_addr, nr_irqs);
    // printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

    // Init network device
    e1000_init();
    printk("> [INIT] E1000 device initialized successfully.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");


    /*
         * Just start kernel with VM and print this string
         * in the first part of task 1 of project 4.
         * NOTE: if you use SMP, then every CPU core should call
         *  `kernel_brake()` to stop executing!
         */
    printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        (unsigned int) get_current_cpu_id());
    wakeup_other_hart();
    // TODO: [p4-task1 cont.] remove the brake and continue to start user processes.
    //kernel_brake();

    // wait for slave hart to init
    while (*slave_hart_lock == 1);
    // Cancel temporary mapping 0x5000_0000 to 0x5100_0000
    cancel_temp_pgdir();

    // exec shell
    char *name = "shell";
    char *argv[] = { "shell" };
    int argc = 1;
    pid_t pid = do_exec(name, argc, argv);      // exec shell
    do_taskset(NULL, pid, 0x3, 1);              // set cpu_mask

    // Setup timer interrupt and enable all interrupt globally
    bios_set_timer(time_base / 100 + get_ticks());

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
