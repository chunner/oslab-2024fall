#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64] = "page_cache_policy = write back\nwrite_back_freq = 30\n\0";

int main(void)
{
    sys_move_cursor(0, 0);
    sys_cd("proc/sys");
    int fd = sys_open("vm", O_RDWR);
    sys_write(fd, buff, strlen(buff));
    printf("[page_cache_policy]: change to write back\n");

    sys_close(fd);
    sys_cd("../..");

    return 0;
}