#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    sys_touch("rcache.txt");
    int fd = sys_open("rcache.txt", O_RDWR);

    // write 'hello world!' * 10
    for (int i = 0; i < 10; i++)
    {
        sys_write(fd, "hello world!\n", 13);
    }

    // read
    sys_move_cursor(0, 0);
    for (int i = 0; i < 10; i++)
    {
        sys_read(fd, buff, 13);
        for (int j = 0; j < 13; j++)
        {
            printf("%c", buff[j]);
        }
    }

    sys_close(fd);

    return 0;
}