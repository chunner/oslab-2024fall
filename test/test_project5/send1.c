#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <mailbox.h>
#define TX_PKT_SIZE 1500

static uint32_t buffer[TX_PKT_SIZE / 4] = {
    0xffffffff, 0x5500ffff, 0xf77db57d, 0x00430008, 0x0000d400, 0x11ff0040,
    0xa8c073d8, 0x00e00101, 0xe914fb00, 0x0004e914, 0x0000,     0x005e0001,
    0x2300fb00, 0x84b7f28b, 0x00450008, 0x0000d400, 0x11ff0040, 0xa8c073d8,
    0x00e00101, 0xe914fb00, 0x0801e914, 0x0000 };
static int len = TX_PKT_SIZE;

#define ETH_ALEN 6u                 // Length of MAC address
#define ETH_P_IP 0x0800u            // IP protocol
// Ethernet header
struct ethhdr {
    uint8_t ether_dmac[ETH_ALEN];   // destination mac address
    uint8_t ether_smac[ETH_ALEN];   // source mac address
    uint16_t ether_type;            // protocol format
};



int main(void)
{
    struct ethhdr eh = {
        .ether_dmac = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff},     // 6 B
        .ether_smac = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53},     // 6 B
        .ether_type = ETH_P_IP                                  // 2 B
    };
    memcpy(buffer, &eh, sizeof(eh));
    *(char *) ((char *) buffer + sizeof(eh)) = 43;   // magic num
    sys_move_cursor(0, 5);
    printf("> [SEND1] start send package.               \n");
    for (int i = 0;i < 16;i++) {
        sys_move_cursor(0, 6);
        // sys_net_send(buffer, len);        // send 1 packet one time
        sys_net_multisend(buffer, 64, len);  // send 1 packet 64 times
        uint32_t checksum = adler32((char *) buffer, len);
        printf("> [SEND1] totally send %d   KB !         \n", (i + 1) * len * 64 / 1024);
        printf("> [SEND1] checksum: %x\n", checksum);
    }
}