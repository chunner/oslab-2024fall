#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <os/mm.h>
#include <os/sched.h>

uint64_t load_task_img(pcb_t *pcb, task_info_t task) {    /* setup code and data segement vm */
    /*----------------------alloc and map vm------------------------------*/
    uint32_t pagenum = NBYTES2PAGE(task.memsize + SECTOR_SIZE); // spare 512 B to handle the offset in image
    uint64_t uva = task.entrypoint;

    uint32_t sector_num = task.sector_num;
    uint32_t sector_pointer = task.firstsector;
    for (int i = 0;i < pagenum;i++) {
        uint64_t kva = alloc_page_helper(uva, pcb->pgdir);
        uva += 0x1000lu;       // 4KB == 1000
        if (sector_num * SECTOR_SIZE >= PAGE_SIZE) {    // read (a page == 8 block) per time
            bios_sd_read(kva, PAGE_SIZE / SECTOR_SIZE, sector_pointer); // mem_address, num_of_blocks, block_id
            sector_num -= PAGE_SIZE / SECTOR_SIZE;
            sector_pointer += PAGE_SIZE / SECTOR_SIZE;
        } else if (sector_num > 0) {                // read remain less than a page 
            bios_sd_read(kva, sector_num, sector_pointer); // mem_address, num_of_blocks, block_id
            sector_num -= sector_num;
            sector_pointer += sector_num;
        }
    }
    set_satp(SATP_MODE_SV39, pcb->pid, kva2pa(pcb->pgdir) >> NORMAL_PAGE_SHIFT);    // Temporarily switch satp
    local_flush_tlb_all();
    // counteract the offset in the image
    uint8_t *dest = (uint8_t *) task.entrypoint;
    uint8_t *src = (uint8_t *) (task.entrypoint + task.offset);
    uint32_t len = SECTOR_SIZE * task.sector_num - task.offset;
    memcpy(dest, src, len);

    return task.entrypoint;
}

