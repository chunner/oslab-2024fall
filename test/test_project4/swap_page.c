#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define FREE_USER_PAGE_NUM 0x10 
#define PAGE_SIZE 4096 // 4K = 0x1000
int main() {
    srand(clock());
    long val = 0;
    uintptr_t mem1 = 20000;
    int curs = 0;
    int i;
    sys_move_cursor(2, 2);
    for (i = 1; i < FREE_USER_PAGE_NUM; i++)
    {
        // sys_move_cursor(2, curs+i);
        val = rand();
        *(long *) mem1 = val;
        printf("<%d>: 0x%lx, %ld\n", i, mem1, val);
        if (*(long *) mem1 != val) {
            printf("Error!\n");
        }
        mem1 += PAGE_SIZE;
    }
    //Only input address.
    //Achieving input r/w command is recommended but not required.
    printf("Success!\n");
    return 0;
}