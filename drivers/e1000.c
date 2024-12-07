#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>
#include <os/sched.h>
#include <os/mm.h>
#include <os/list.h>

#define E1000_TCTL_CT_SHIFT   4 
#define E1000_TCTL_COLD_SHIFT   12 

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = { 0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53 };


extern list_head recv_block_queue;
extern list_head send_block_queue;

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
    /* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

    /* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

    /**
     * Delay to allow any outstanding PCI transactions to complete before
     * resetting the device
     */
    latency(1);

    /* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR));
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i = 0;i < TXDESCS;i++) {
        tx_desc_array[i].addr = kva2pa((uint64_t) tx_pkt_buffer[i]);
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
    }
    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    uint64_t tx_desc_base = kva2pa((uintptr_t) tx_desc_array);
    uint32_t lower_base_addr = (uint32_t) (tx_desc_base & UINT32_MAX);
    uint32_t higher_base_addr = (uint32_t) (tx_desc_base >> 32);
    uint32_t array_size = TXDESCS * sizeof(struct e1000_tx_desc);
    e1000_write_reg(e1000, E1000_TDBAL, lower_base_addr);
    e1000_write_reg(e1000, E1000_TDBAH, higher_base_addr);
    e1000_write_reg(e1000, E1000_TDLEN, array_size);
    /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);
    /* TODO: [p5-task1] Program the Transmit Control Register */
    uint32_t tctl_val = E1000_TCTL_EN | E1000_TCTL_PSP | (0x10 << E1000_TCTL_CT_SHIFT) | (0X40 << E1000_TCTL_COLD_SHIFT);
    e1000_write_reg(e1000, E1000_TCTL, tctl_val);

    local_flush_dcache();       // flush the cache after write txd
    // printl("TCTL: %x\n", e1000_read_reg(e1000, E1000_TCTL));
    // printl("TDBAL: %x\n", e1000_read_reg(e1000, E1000_TDBAL));
    // printl("TDBAH: %x\n", e1000_read_reg(e1000, E1000_TDBAH));
    // printl("TDLEN: %x\n", e1000_read_reg(e1000, E1000_TDLEN));
    // printl("TDH: %x\n", e1000_read_reg(e1000, E1000_TDH));
    // printl("TDT: %x\n", e1000_read_reg(e1000, E1000_TDT));
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    e1000_write_reg_array(e1000, E1000_RA, 0, *(uint32_t *) enetaddr);      // 4 Bytes
    e1000_write_reg_array(e1000, E1000_RA, 1, (*(uint16_t *) (enetaddr + 4)) | E1000_RAH_AV);    // 2 Bytes
    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0;i < RXDESCS;i++) {
        rx_desc_array[i].addr = kva2pa((uint64_t) rx_pkt_buffer[i]);
        rx_desc_array[i].special = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].length = 0;
    }
    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uint64_t rx_desc_base = kva2pa((uintptr_t) rx_desc_array);
    uint32_t lower_base_addr = (uint32_t) (rx_desc_base & UINT32_MAX);
    uint32_t higher_base_addr = (uint32_t) (rx_desc_base >> 32);
    uint32_t array_size = TXDESCS * sizeof(struct e1000_rx_desc);
    e1000_write_reg(e1000, E1000_RDBAL, lower_base_addr);
    e1000_write_reg(e1000, E1000_RDBAH, higher_base_addr);
    e1000_write_reg(e1000, E1000_RDLEN, array_size);
    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);
    /* TODO: [p5-task2] Program the Receive Control Register */
    uint32_t rctl_val = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048;
    e1000_write_reg(e1000, E1000_RCTL, rctl_val);
    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0);

    local_flush_dcache();       // flush the cache after write rxd
    // printl("RCTL: %x\n", e1000_read_reg(e1000, E1000_RCTL));
    // printl("RDBAL: %x\n", e1000_read_reg(e1000, E1000_RDBAL));
    // printl("RDBAH: %x\n", e1000_read_reg(e1000, E1000_RDBAH));
    // printl("RDLEN: %x\n", e1000_read_reg(e1000, E1000_RDLEN));
    // printl("RDH: %x\n", e1000_read_reg(e1000, E1000_RDH));
    // printl("RDT: %x\n", e1000_read_reg(e1000, E1000_RDT));
    // printl("RAL: %x\n", e1000_read_reg_array(e1000, E1000_RA, 0));
    // printl("RAH: %x\n", e1000_read_reg_array(e1000, E1000_RA, 1));
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    // printl("TCTL: %x\n", e1000_read_reg(e1000, E1000_TCTL));
    // printl("TDBAL: %x\n", e1000_read_reg(e1000, E1000_TDBAL));
    // printl("TDBAH: %x\n", e1000_read_reg(e1000, E1000_TDBAH));
    // printl("TDLEN: %x\n", e1000_read_reg(e1000, E1000_TDLEN));
    // printl("TDH: %x\n", e1000_read_reg(e1000, E1000_TDH));
    // printl("TDT: %x\n", e1000_read_reg(e1000, E1000_TDT));
    /* TODO: [p5-task1] Transmit one packet from txpacket */

    local_flush_dcache();       // flush the cache before read txd
    int index = e1000_read_reg(e1000, E1000_TDT);
    while (tx_desc_array[index].status & E1000_TXD_STAT_DD == 0) {
        uint32_t ims_val = e1000_read_reg(e1000, E1000_IMS);
        if (ims_val & E1000_IMS_TXQE == 0) {    // if not enabled TXQE interrupt
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        }
        do_block(&current_running->list, &send_block_queue);
        do_scheduler();
        local_flush_dcache();       // flush the cache before read txd
    };   // wait until there is a free descriptor

    int transmit_len = length > TX_PKT_SIZE ? TX_PKT_SIZE : length;
    tx_desc_array[index].length = transmit_len;
    if (length > TX_PKT_SIZE) {
        tx_desc_array[index].cmd = E1000_TXD_CMD_RS;
    } else {
        tx_desc_array[index].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
    }
    tx_desc_array[index].status = 0;
    check_uva_mem(txpacket, length, current_running);
    memcpy(pa2kva(tx_desc_array[index].addr), txpacket, transmit_len);
    e1000_write_reg(e1000, E1000_TDT, (index + 1) % TXDESCS);   // update TDT

    // printl("-----------------------send: %d------------------\n", transmit_len);
    // char *curr = (char *) tx_pkt_buffer[index];
    // for (int j = 0; j < (transmit_len + 15) / 16; ++j) {
    //     for (int k = 0; k < 16 && (j * 16 + k < transmit_len); ++k) {
    //         printl("%02x ", (uint32_t) (*(uint8_t *) curr));
    //         ++curr;
    //     }
    //     printl("\n");
    //     //if (curr - tx_pkt_buffer[index] >= 80) break;
    // }

    local_flush_dcache();       // flush the cache after write txd and tx buffer
    return transmit_len;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    int poll_len = 0;
    int eop = 0;
    do {
        local_flush_dcache();       // flush the cache before read rxd
        int index = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;
        while ((rx_desc_array[index].status & E1000_RXD_STAT_DD) == 0) {
            do_block(&current_running->list, &recv_block_queue);
            do_scheduler();
            local_flush_dcache();       // flush the cache before read rxd
        };   // wait until there is a packet

        poll_len += rx_desc_array[index].length;
        check_uva_mem(rxbuffer, poll_len, current_running);
        memcpy((char *) rxbuffer, rx_pkt_buffer[index], poll_len);
        eop = rx_desc_array[index].status & E1000_RXD_STAT_EOP;

        rx_desc_array[index].special = 0;
        rx_desc_array[index].errors = 0;
        rx_desc_array[index].status = 0;
        rx_desc_array[index].csum = 0;
        rx_desc_array[index].length = 0;
        e1000_write_reg(e1000, E1000_RDT, index);   // update RDT

        // char *curr = (char *) rx_pkt_buffer[index];
        // printl("--------------------------recv: %d------------------\n", poll_len);
        // for (int j = 0; j < (poll_len + 15) / 16; ++j) {
        //     for (int k = 0; k < 16 && (j * 16 + k < poll_len); ++k) {
        //         printl("%02x ", (uint32_t) (*(uint8_t *) curr));
        //         ++curr;
        //     }
        //     printl("\n");
        //     //if (curr - rx_pkt_buffer[index] >= 80) break;
        // }

        local_flush_dcache();       // flush the cache after read rxd and rx buffer
    } while (!eop);
    return poll_len;
}