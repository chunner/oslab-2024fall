#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define nmultask 4
#define COMMAND_LEN 50


#define task_info_new_loc 0x58000010  // user's sp + 0x10
#define kernel          0x50201000
#define tasknum_loc     0x502001f6



int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

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

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
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
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);


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
        asm volatile("wfi");
    }

    return 0;
}
