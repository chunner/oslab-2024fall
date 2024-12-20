#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    sys_touch("rcache.txt");
    int fd = sys_open("rcache.txt", O_RDWR);

    // write 'hello world!' * 10
    for (int i = 0; i < 5; i++)
    {
        sys_write(fd, "hello world!\n", 13);
    }
    uint64_t old_clk;
    uint64_t new_clk;
    // read with cache
    sys_move_cursor(0, 0);
    old_clk = sys_get_tick();
    for (int i = 0; i < 5; i++)
    {
        sys_read(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }
    new_clk = sys_get_tick();
    printf("[read with cache] time: %d\n", new_clk - old_clk);
    // read without cache
    sys_bflush();
    sys_init_bcache();
    old_clk = sys_get_tick();
    sys_lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < 5; i++)
    {
        sys_read(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }
    new_clk = sys_get_tick();
    printf("[read without cache] time: %d\n", new_clk - old_clk);





    sys_close(fd);

    return 0;
}