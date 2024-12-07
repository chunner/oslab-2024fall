#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define RXDESCS 64          // Number of rx descriptors
#define RX_PKT_SIZE 2048

static int recv_length[RXDESCS];
static uint32_t recv_buffer[RXDESCS * RX_PKT_SIZE / 4];


int main(void)
{
    int print_location = 1;
    int total_KB = 0;

    sys_move_cursor(0, print_location);
    printf("[RECV1] start recv: ");

    uint64_t time_base = sys_get_timebase();
    uint64_t cnt = 0;

    while (1)
    {
        uint64_t old_clk = sys_get_tick();
        int ret = sys_net_recv(recv_buffer, RXDESCS, recv_length); // receive 64 packets one time
        total_KB += ret / 1024;
        ++cnt;
        // speed caculation
        uint64_t new_clk = sys_get_tick();
        uint64_t clks = (new_clk - old_clk);
        uint64_t speed = ret * time_base / clks / 1000;      // KB/s

        sys_move_cursor(0, print_location + 3);
        printf("<%d>: ret = %x, clk = %x, timebase = %d, speed = %dKB/s\n", cnt, ret, clks, time_base, speed);
        printf("> [RECV1] totally recieve %d KB !         \n", total_KB);
        // uint32_t checksum = adler32((char *) recv_buffer, recv_length);
        // printf("> [RECV1] checksum: %x\n", checksum);



        char *curr = (char *) recv_buffer;
        char *next = curr;
        for (int i = 0; i < RXDESCS; ++i) {
            next += recv_length[i];
            if (curr[14] != 43) {       // check for magic num
                curr = next;
                continue;
            }
            uint32_t checksum = adler32(curr, recv_length[i]);
            sys_move_cursor(0, print_location + 5);
            printf("> [RECV1] checksum: %x", checksum);
            curr = next;
        }
    }

    return 0;
}