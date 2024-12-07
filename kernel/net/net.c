#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    int total_len = 0;
    while (length > 0) {
        int trans_len = e1000_transmit(txpacket, length);
        txpacket += trans_len;
        length -= trans_len;
        total_len += trans_len;
        if (trans_len == 0) {
            uint32_t ims_val = e1000_read_reg(e1000, E1000_IMS);
            if (ims_val & E1000_IMS_TXQE == 0) {    // if not enabled TXQE interrupt
                e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
            }
            do_block(&current_running->list, &send_block_queue);
            do_scheduler();
        }
    }
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full

    return total_len;  // Bytes it has transmitted
}

int do_net_multisend(void *txbuffer, int pkt_num, int pkt_len)
{
    int toltal_len = 0;
    for (int i = 0; i < pkt_num; i++) {
        int trans_len = e1000_transmit(txbuffer, pkt_len);
        toltal_len += trans_len;
        if (trans_len == 0) {
            do_block(&current_running->list, &send_block_queue);
            do_scheduler();
            i--;
        }
    }
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return toltal_len;  // Bytes it has received


}




int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    int recv_len = 0;
    for (int i = 0; i < pkt_num; i++) {
        pkt_lens[i] = e1000_poll(rxbuffer);
        recv_len += pkt_lens[i];
        rxbuffer += pkt_lens[i];
        if (pkt_lens[i] == 0) {
            do_block(&current_running->list, &recv_block_queue);
            do_scheduler();
            i--;
        }
    }
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return recv_len;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
    uint32_t icr_val = e1000_read_reg(e1000, E1000_ICR);
    uint32_t ims_val = e1000_read_reg(e1000, E1000_IMS);
    if ((icr_val & E1000_ICR_TXQE) & (ims_val & E1000_IMS_TXQE)) {
        while (send_block_queue.next != &send_block_queue) {
            do_unblock(send_block_queue.next);
        }
        e1000_write_reg(e1000, E1000_IMC, E1000_IMS_TXQE); // disable TXQE interrupt
    }
    if ((icr_val & E1000_ICR_RXDMT0) & (ims_val & E1000_IMS_RXDMT0)) {
        while (recv_block_queue.next != &recv_block_queue) {
            do_unblock(recv_block_queue.next);
        }
    }
}