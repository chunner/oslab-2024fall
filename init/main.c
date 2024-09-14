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

// Task info array
task_info_t tasks[TASK_MAXNUM];

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

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
        /* get taskinfo from memory */
    short *tasknum_mem = (short *) tasknum_loc;
    tasknum = * tasknum_mem;
    task_info_t *taskinfo_mem = (task_info_t *)(task_info_new_loc);
    for(int i = 0; i < tasknum; i++){
        tasks[i].sector_num = taskinfo_mem[i].sector_num;
        tasks[i].firstsector = taskinfo_mem[i].firstsector;
        tasks[i].offset = taskinfo_mem[i].offset;
        tasks[i].entry = taskinfo_mem[i].entry;
        strcpy(tasks[i].taskname,taskinfo_mem[i].taskname);
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
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */


    /* TODO: [p2-task1] remember to initialize 'current_running' */

}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
}
/************************************************************/

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
    



    uint64_t usrentry;      // the entry of app
    while(1){
        bios_putstr("Please input taskname: 0.bss; 1.auipc; 2.data; 3.2048; 4.output1; 5.sort2; 6.dedup3; 7.append\n\r");

        int input;
        char taskname[COMMAND_LEN];
        int i=0;
        while(i<=COMMAND_LEN){
            input = port_read_ch();   
            if (input >= 0 && input <= 127) {  // if the input is not among ASCII
                bios_putchar(input);
                if (input == '\n' || input == '\r') {
                    taskname[i++] = '\0';
                    break;
                } 
                taskname[i++] = input;
            }
        }
        bios_putstr("\n\r");         // new line
        if(strncmp(taskname, "multitask", 9) == 0){      // len of "multitask" == 9, multitask
            char multask[nmultask][MAXLEN];
            int n = 0;  // the number of tasks
            int k = 0;
            for(int i =  9; taskname[i] != '\0'; i++){ // taskname start from taskname[10]
                if(taskname[i] == ' '){
                    multask[n-1][k] = '\0';
                    n ++;
                    k = 0;
                }else {
                    multask[n-1][k++] = taskname[i]; 
                }
            }
            multask[n-1][k] = '\0';
            for(int i = 0; i< n; i++){
                usrentry = load_task_img(multask[i]);
                if(usrentry != -1){                // task name input correct
                    // 使用内联汇编执行 JAL 跳转到 usrentry
                    __asm__ __volatile__ (
                        "jalr ra, %0\n"
                        :
                        : "r"(usrentry)
                    );
                }
            }
        }else{          // single task
            usrentry = load_task_img(taskname);
            if(usrentry != -1){                // task name input correct
                // 使用内联汇编执行 JALR 跳转到 usrentry
                __asm__ __volatile__ (
                    "jalr ra, %0\n"
                    :
                    : "r"(usrentry)
                );
            }
        }
    }


    
    // int input;
    
    // while(1){
    //     input = bios_getchar();   
    //     if (input < 0 || input > 127) {  // if the input is not among ASCII
    //         continue;              // skip illegal ch
    //     }
    //     bios_putchar(input);       
    // }

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

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
