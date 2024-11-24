#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define PAGE_SIZE 4096 // 4K = 0x1000
enum prot {
    PROT_NONE,
    PROT_READ,
    PROT_WRITE,
    PROT_EXEC
};
unsigned char code[] = {
    // // NOP 指令 : 填充无操作指令
    // 0x13, 0x00, 0x00, 0x00,  // NOP: addi x0, x0, 0
    0x80, 0x82
    // RET 指令 (jalr x1, x0, 0): 跳转到 x1 寄存器保存的返回地址
    // 0x00, 0x00, 0x00, 0xF7,  // jalr x1, x0, 0 -> 返回到保存的地址
};
int main() {

    long value;
    uintptr_t mem1 = 0x20000;
    uintptr_t mem2 = 0x21000;
    void (*func)() = (void (*)())mem2;
    sys_move_cursor(0, 2);

    *(long *) mem1 = 12;
    memcpy(mem2, code, sizeof(code));

    printf("--------------------test begin\n");
    /* -------------------------------------test prot noe */
    sys_mprotect(mem1, PAGE_SIZE + 1, PROT_NONE);
    printf("try to prot_none:\n");
    value = *(long *) mem2;
    memcpy(mem2, code, sizeof(code));
    func();
    /* -------------------------------------test prot read */
    sys_mprotect(mem1, PAGE_SIZE + 1, PROT_READ);
    printf("try to prot_read:\n");
    value = *(long *) mem2;
    memcpy(mem2, code, sizeof(code));
    func();
    /* -------------------------------------test prot write */
    sys_mprotect(mem1, PAGE_SIZE + 1, PROT_WRITE);
    printf("try to prot_write:\n");
    value = *(long *) mem2;
    memcpy(mem2, code, sizeof(code));
    func();
    /* -------------------------------------test prot write */
    sys_mprotect(mem1, PAGE_SIZE + 1, PROT_EXEC);
    printf("try to prot_exec:\n");
    value = *(long *) mem2;
    memcpy(mem2, code, sizeof(code));
    func();
    return 0;
}
