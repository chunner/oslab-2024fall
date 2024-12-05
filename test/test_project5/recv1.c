#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define MAX_RECV_CNT 32
#define RX_PKT_SIZE 256

static uint32_t recv_buffer[MAX_RECV_CNT * RX_PKT_SIZE];
static int recv_length[MAX_RECV_CNT];

int main(void)
{
    int print_location = 1;
    int total_package = 0;

    sys_move_cursor(0, print_location);
    printf("[RECV1] start recv(%d): ", MAX_RECV_CNT);

    while (1)
    {
        int ret = sys_net_recv(recv_buffer, MAX_RECV_CNT, recv_length);
        char *curr = (char *) recv_buffer;
        for (int i = 0; i < MAX_RECV_CNT; ++i) {
            if (curr[14] != 43) {
                continue;
            }
            sys_move_cursor(0, print_location + 1);
            uint32_t checksum = adler32(curr, recv_length[i]);
            printf("> [RECV1] totally recieve package %d/%d !         \n", ++total_package, 1024);
            printf("> [RECV1] checksum: %x\n", checksum);
            for (int j = 0; j < (recv_length[i] + 15) / 16; ++j) {
                for (int k = 0; k < 16 && (j * 16 + k < recv_length[i]); ++k) {
                    printf("%02x ", (uint32_t) (*(uint8_t *) curr));
                    ++curr;
                }
                printf("\n");
            }
        }
    }

    return 0;
}