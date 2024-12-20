#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    sys_touch("rcache.txt");
    int fd = sys_open("rcache.txt", O_RDWR);

    int offset = 0;
    // write 'hello world!' * 10
    for (int i = 0; i < 10; i++)
    {
        sys_lseek(fd, offset, SEEK_SET);
        offset += 4096;
        sys_write(fd, "hello world!\n", 13);
    }
    uint64_t old_clk;
    uint64_t new_clk;
    // read with cache
    sys_move_cursor(0, 0);
    offset = 0;
    old_clk = sys_get_tick();
    for (int i = 0; i < 10; i++)
    {
        sys_lseek(fd, offset, SEEK_SET);
        offset += 4096;
        sys_read(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }
    new_clk = sys_get_tick();
    uint64_t t1 = new_clk - old_clk;
    printf("[read with cache] time: t1 = %d\n", t1);
    // read without cache
    sys_bflush();
    sys_init_bcache();
    offset = 0;
    old_clk = sys_get_tick();
    sys_lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < 10; i++)
    {
        sys_lseek(fd, offset, SEEK_SET);
        offset += 4096;
        sys_read(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }
    new_clk = sys_get_tick();
    uint64_t t2 = new_clk - old_clk;
    printf("[read without cache] time: t2 = %d\n", new_clk - old_clk);

    uint64_t speedup = t2 * 100 / t1;
    printf("t2/t1 = %d%%\n", speedup);


    sys_close(fd);

    return 0;
}