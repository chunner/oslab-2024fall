#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#define PAGE_SIZE 4096 // 4K = 0x1000
enum prot {
    PROT_NONE,
    PROT_READ,
    PROT_WRITE,
    PROT_EXEC
};
#define BUF_LEN 20
#define PAGE_SIZE 4096 // 4K = 0x1000

int main(int argc, char *argv[]) {
    assert(argc >= 2);
    int print_location = (argc == 2) ? 0 : atoi(argv[2]);
    sys_move_cursor(0, print_location);

    long value;
    uintptr_t mem1 = malloc(PAGE_SIZE * 2);;
    uintptr_t mem2 = mem1 + PAGE_SIZE;
    void (*func)() = (void (*)())mem2;

    *(long *) mem1 = 12;
    *(long *) mem2 = 0x8082;

    if (argv[1][0] == 1 + '0') {     // read
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_NONE);
        printf("try to prot_none:");
        value = *(long *) mem2;
    } else if (argv[1][0] == 2 + '0') {  //write
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_NONE);
        *(long *) mem2 = 0x8082;
    } else if (argv[1][0] == 3 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_NONE);
        func();
    } else if (argv[1][0] == 4 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_READ);
        printf("try to prot_read:");
        value = *(long *) mem2; // read
    } else if (argv[1][0] == 5 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_READ);
        *(long *) mem2 = 0x8082;   // wirte
    } else if (argv[1][0] == 6 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_READ);
        func();
    } else if (argv[1][0] == 7 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_EXEC);
        printf("try to prot_exec:");
        value = *(long *) mem2; // read
    } else if (argv[1][0] == 8 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_EXEC);
        *(long *) mem2 = 0x8082;   // wirte
    } else if (argv[1][0] == 9 + '0') {
        sys_mprotect(mem1, PAGE_SIZE + 1, PROT_EXEC);
        func();
    } else if (argv[1][0] == 0 + '0') {
        for (int i = 1;i <= 9;i++) {
            char buf_location[BUF_LEN];
            char buf_handle[BUF_LEN];
            assert(itoa(i, buf_location, BUF_LEN, 10) != -1);
            assert(itoa(i, buf_handle, BUF_LEN, 10) != -1);
            int print_loc = i;
            char *argv[] = { "mprotect", buf_handle, buf_location };
            int argc = 3;
            sys_exec(argv[0], 3, argv);
        }
    }
    return 0;
}
