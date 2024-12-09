#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define RXDESCS 8          // Number of rx descriptors
#define RX_PKT_SIZE 2048

static int recv_length[RXDESCS];
static uint32_t recv_buffer[RXDESCS * RX_PKT_SIZE / 4];


int main(void)
{
    int print_location = 1;
    int total_packets = 0;
    int total_checksum = 0;

    sys_move_cursor(0, print_location);
    printf("[RECV1] start recv: ");

    uint64_t time_base = sys_get_timebase();
    uint64_t cnt = 0;
    uint64_t old_clk;
    uint64_t new_clk;
    uint64_t total_B = 0;
    old_clk = sys_get_tick();
    for (int i = 0;i < 16 * 8;i++)
    {
        int ret = sys_net_recv(recv_buffer, RXDESCS, recv_length); // receive 64 packets one time
        total_packets += RXDESCS;
        total_B += ret;
        // total_KB += ret / 1024;
        // ++cnt;
        // speed caculation
        //uint64_t new_clk = sys_get_tick();
        //uint64_t clks = (new_clk - old_clk);
        //uint64_t speed = ret * time_base / clks / 1000;      // KB/s

        //old_clk = new_clk;

        //sys_move_cursor(0, print_location + 3);
        //  printf("<%d>: ret = %x, clk = %x, timebase = %d, speed = %dKB/s\n", cnt, ret, clks, time_base, speed);
         // printf("> [RECV1] totally recieve %d KB !         \n", total_KB);
         // printf("> [RECV1] totally recieve %d packets !         \n", total_packets);
         // uint32_t checksum = adler32((char *) recv_buffer, recv_length);
         // printf("> [RECV1] checksum: %x\n", checksum);



        char *curr = (char *) recv_buffer;
        char *next = curr;
        for (int j = 0; j < RXDESCS; ++j) {
            next += recv_length[j];
            // if (curr[14] != 43) {       // check for magic num
            //     curr = next;
            //     continue;
            // }
            uint32_t checksum = adler32(curr, 1500); //recv_length[i]);
            total_checksum += checksum;
            // sys_move_cursor(0, print_location + 7);
            // printf("> [RECV1] total_checksum: %x\n", total_checksum);
            // printf("> [RECV1] checksum: %x\n", checksum);
            curr = next;
        }
        //printf("> [RECV1] checksum: %x\n", checksum);
    }
    new_clk = sys_get_tick();
    uint64_t clks = (new_clk - old_clk);
    uint64_t total_KB = total_B / 1024;
    uint64_t speed = total_KB * time_base / clks;      // KB/s


    sys_move_cursor(0, print_location + 3);
    printf("> [RECV1] totally recieve %d KB !         \n", total_KB);
    printf("> [RECV1] totally recieve %d packets !         \n", total_packets);
    printf("> [RECV1] speed: %dKB/s\n", speed);
    printf("> [RECV1] total_checksum: %x\n", total_checksum);

    return 0;
}