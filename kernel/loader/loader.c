#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define task_info_new_loc 0x58000010  // user's sp + 0x10
#define kernel          0x50201000
#define tasknum_loc     0x502001f6
uint64_t load_task_img(char taskname[])
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    short * tasknum = (short *) tasknum_loc;
    task_info_t *taskinfo = (task_info_t *)(task_info_new_loc);
    int i;      // task index
    for(i = 0; i < * tasknum; i ++){
        if(strcmp(taskname, taskinfo[i].taskname) == 0){
            break;
        }
    }
    if(i == *tasknum){             // task name match failed
        port_write("taskname does not exit\n\r");
        return -1;
    }

    bios_sd_read(taskinfo[i].entry, taskinfo[i].sector_num, taskinfo[i].firstsector);         
    // mem_address = 0x52000000 + 10000* taskid, num_of_blocks == 15, block_id = taskid * 15 + 15: bootloder(0), kernel(1-15), task(16+)
    return taskinfo[i].entry + taskinfo[i].offset;
}