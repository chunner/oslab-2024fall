#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define MAX_RECV_CNT 64
#define RX_PKT_SIZE 300

static uint32_t recv_buffer[MAX_RECV_CNT * RX_PKT_SIZE];
static int recv_length[MAX_RECV_CNT];

int main(void)
{
    int print_location = 1;
    int total_package = 0;
    int len_lim = 80;

    sys_move_cursor(0, print_location);
    printf("[RECV1] start recv(%d): ", MAX_RECV_CNT);
    uint64_t time_base = sys_get_timebase();
    uint64_t old_clk = sys_get_tick();
    uint64_t cnt = 0;
    while (1)
    {
        int ret = sys_net_recv(recv_buffer, MAX_RECV_CNT, recv_length);
        // speed caculation
        ++cnt;
        uint64_t new_clk = sys_get_tick();
        uint64_t clks = (new_clk - old_clk);
        uint32_t speed = ret * time_base / clks / 1000;      // KB/s
        old_clk = new_clk;
        sys_move_cursor(0, print_location + 7);
        printf("<%d>: ret = %x, clk = %x, speed = %x\n", cnt, ret, clks, speed);
        char *curr = (char *) recv_buffer;
        char *next = curr;
        for (int i = 0; i < MAX_RECV_CNT; ++i) {
            next += recv_length[i];
            if (curr[14] != 43) {       // check for magic num
                curr = next;
                continue;
            }
            if (recv_length[i] > len_lim)
                recv_length[i] = len_lim;
            sys_move_cursor(0, print_location + 1);
            uint32_t checksum = adler32(curr, 1024);
            printf("> [RECV1] totally recieve package %d/%d !         \n", ++total_package, 1024);
            printf("> [RECV1] speed: %dKB/s\n", speed);
            printf("> [RECV1] checksum: %x\n", checksum);
            // for (int j = 0; j < (recv_length[i] + 15) / 16; ++j) {
            //     for (int k = 0; k < 16 && (j * 16 + k < recv_length[i]); ++k) {
            //         printf("%02x ", (uint32_t) (*(uint8_t *) curr));
            //         ++curr;
            //     }
            //     printf("\n");
            // }
            curr = next;
        }
    }

    return 0;
}