#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    bios_sd_read(USER_ENTRYPOINT, 15,  taskid * 15 + 16);         
    // mem_address = 0x52000000, num_of_blocks == 15, block_id = taskid * 15 + 15: bootloder(0), kernel(1-15), task(16+)
    return USER_ENTRYPOINT;
}