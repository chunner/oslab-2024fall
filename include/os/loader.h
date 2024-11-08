#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/sched.h>

uint64_t load_task_img(pcb_t *pcb, task_info_t task);

#endif