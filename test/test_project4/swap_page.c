#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define PAGE_NUM 0x8 
#define PAGE_SIZE 4096 // 4K = 0x1000
int main() {
    srand(clock());
    uintptr_t mem1 = 0x20000;
    int curs = 0;
    int i;
    sys_move_cursor(2, 2);
    long val[PAGE_NUM];
    for (i = 0; i < PAGE_NUM; i++)
    {
        // sys_move_cursor(2, curs+i);
        val[i] = rand();
        *(long *) mem1 = val[i];
        printf("<%d>: 0x%lx, %ld\n", i, mem1, val[i]);
        if (*(long *) mem1 != val[i]) {
            printf("Error!\n");
            return 0;
        }
        mem1 += PAGE_SIZE;
    }
    printf("--------------------write Success!\n");
    uintptr_t mem2 = 20000;
    for (i = 1; i < PAGE_NUM; i++)
    {
        // sys_move_cursor(2, curs+i);
        printf("<%d>: 0x%lx, %ld\n", i, mem1, *(long *) mem1);
        if (*(long *) mem1 != val[i]) {
            printf("Error!\n");
            return 0;
        }
        mem1 += PAGE_SIZE;
    }
    printf("--------------------read Success!\n");
    return 0;
}