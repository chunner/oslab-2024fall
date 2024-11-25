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
    // uintptr_t mem1 = 0x20000;
    // uintptr_t mem2 = 0x20000;
    int curs = 0;
    int i;
    while (1) {
        uintptr_t mem1 = malloc(PAGE_SIZE * PAGE_NUM);
        uintptr_t mem2 = mem1;
        sys_move_cursor(0, 2);
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
        for (i = 0; i < PAGE_NUM; i++)
        {
            // sys_move_cursor(2, curs+i);
            printf("<%d>: 0x%lx, %ld\n", i, mem2, *(long *) mem2);
            if (*(long *) mem2 != val[i]) {
                printf("Error!\n");
                return 0;
            }
            mem2 += PAGE_SIZE;
        }
        printf("--------------------read Success!\n");
        sys_sleep(1);
    }
    return 0;
}