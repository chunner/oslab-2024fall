#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <os/mm.h>

uint64_t load_task_img(int taskid)
{
    uint64_t memaddr = tasks[taskid].entrypoint;
    uint32_t sector_num = tasks[taskid].sector_num;
    uint32_t sector_pointer = tasks[taskid].firstsector;
    while (sector_num * SECTOR_SIZE > PAGE_SIZE) {      // read a page per time
        bios_sd_read(memaddr, PAGE_SIZE / SECTOR_SIZE, sector_pointer); // mem_address, num_of_blocks, block_id
        memaddr += PAGE_SIZE;
        sector_num -= PAGE_SIZE / SECTOR_SIZE;
        sector_pointer += PAGE_SIZE / SECTOR_SIZE;
    }
    if (sector_num > 0) {   // Reading less than a page
        bios_sd_read(memaddr, sector_num, sector_pointer);
    }
    // counteract the offset in the image
    uint8_t *dest = (uint8_t *) tasks[taskid].entrypoint;
    uint8_t *src = (uint8_t *) (tasks[taskid].entrypoint + tasks[taskid].offset);
    uint32_t len = SECTOR_SIZE * tasks[taskid].sector_num - tasks[taskid].offset;
    memcpy(dest, src, len);

    return tasks[taskid].entrypoint;
}

