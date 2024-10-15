#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

#define MAX_NAME_LEN 44           // max len of task name

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {    // 64 byte
    //int taskid;
    char taskname[MAX_NAME_LEN];    //    44 byte
    int sector_num;     // 4 byte
    int firstsector;    // 4 byte
    int offset;         // 4 byte 
    uint64_t entry;     // 8 byte
} task_info_t;


extern task_info_t tasks[TASK_MAXNUM];
extern short tasknum;
#endif