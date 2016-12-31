#include "lpc17xx.h"
#include "arp.h"
#include "memory.h"
#include "byteswap.h"
#include "init.h"
#include <string.h>

#define DESC_LEN 12
#define RX_FRAG_BUF_SZ 127
#define PHY_ADDR 1

static uint8_t mac_address[ETHER_ADDR_LEN] = {0, 1, 2, 3, 4, 5};

typedef struct
{
    void *packet;
    uint32_t control;
} txrx_descriptor;

typedef struct
{
    uint32_t status_info;
    uint32_t status_hash_crc;
} rx_status_t;

/* txrx descriptor arrays. */
txrx_descriptor __attribute__((aligned(4))) tx_desc[DESC_LEN];
txrx_descriptor __attribute__((aligned(4))) rx_desc[DESC_LEN];
uint32_t        __attribute__((aligned(4))) tx_status[DESC_LEN];
rx_status_t     __attribute__((aligned(8))) rx_status[DESC_LEN];

/* Our ethernet address, set at init time. */
uint8_t ether_addr[ETHER_ADDR_LEN];

static void phy_write(int reg, int writeval)
{
    LPC_EMAC->MCMD = 0;
    LPC_EMAC->MADR = reg | (PHY_ADDR << 8);
    LPC_EMAC->MWTD = writeval;
    while (LPC_EMAC->MIND & 0x1) {};
}

static unsigned short phy_read(unsigned char reg)
{
    LPC_EMAC->MCMD = 0x1;
    LPC_EMAC->MADR = reg | (PHY_ADDR << 8);
    while  (LPC_EMAC->MIND & 0x1) {};
    LPC_EMAC->MCMD = 0;
    return (LPC_EMAC->MRDD);
}

void irq_enet()
{
    static void *current_frame = 0;
    static int current_frame_len = 0;

    while (LPC_EMAC->RxConsumeIndex != LPC_EMAC->RxProduceIndex) {
        int desc_idx = LPC_EMAC->RxConsumeIndex,
            frag_len = (rx_status[desc_idx].status_info & 0x7FF) + 1;
        void *frag = rx_desc[desc_idx].packet;

        /* Gather the new fragment out of DMA memory. */
        if (current_frame) {
            void *new_frame_buf;

            new_frame_buf = get_mem(current_frame_len + frag_len);
            memcpy(new_frame_buf, current_frame, current_frame_len);
            memcpy(new_frame_buf + frag_len, frag, frag_len);

            free_mem(current_frame);

            current_frame_len += frag_len;
            current_frame = new_frame_buf;
        } else {
            current_frame = get_mem(frag_len);
            memcpy(current_frame, frag, frag_len);
            current_frame_len = frag_len;
        }

        /* Do we have a full frame? */
        if (rx_status[desc_idx].status_info & (1 << 30)) {

            /* We do, process it. */
            ether_rx_frame(current_frame, current_frame_len);

            free_mem(current_frame);
            current_frame = 0;
            current_frame_len = 0;
        }

        desc_idx += 1;
        desc_idx %= DESC_LEN;
        LPC_EMAC->RxConsumeIndex = desc_idx;
    }

    LPC_EMAC->IntClear = (1 << 3);
}

void emac_init()
{
    int i;
    uint16_t link_params;

    /* Enable ethernet power. */
    LPC_SC->PCONP |= (1 << 30);

    /* Enable ethernet pins. */
    LPC_PINCON->PINSEL2 = 0x50150105;
    LPC_PINCON->PINSEL3 = 0x5;

    /* Bring the MAC out of SOFT RESET */
    LPC_EMAC->MAC1 = 0;

    /* Enable automatic CRC, PADding.  */
    LPC_EMAC->MAC2 |= 1 | (1 << 4) | (1 << 5);

    /* Set the interframe gap time */
    LPC_EMAC->IPGT = 0x15;
    LPC_EMAC->IPGR = 0x12 | (0xC << 8);

    /* Enable RMII. */
    LPC_EMAC->Command |= (1 << 9);

    /* Reset the PHY. */
    phy_write(0, (1 << 15));

    /* Wait for the PHY to come out of reset. */
    while(phy_read(0) & (1 << 15)) {};

    /* Enable auto negotiation */
    phy_write(0, (1 << 12));

    /* Wait for link to become ready and auto negotiation to
     * complete. */
    while (!(phy_read(0x10) & ((1 << 0) | (1 << 4)))) {};

    /* Get link parameters */
    link_params = phy_read(0x10);

    if (!(link_params & (1 << 1)))
        /* Link speed is 100 Mbps. */
        LPC_EMAC->SUPP = (1 << 8);
    else
        /* Link speed is 10 Mbps. */
        LPC_EMAC->SUPP = 0;

    if (link_params & (1 << 2))
        /* Link is full-duplex. */
        LPC_EMAC->Command |= (1 << 10);
    else
        LPC_EMAC->Command &= ~(1 << 10);

    /* Set the station address. */
    LPC_EMAC->SA0 = (mac_address[0] << 8) | mac_address[1];
    LPC_EMAC->SA1 = (mac_address[2] << 8) | mac_address[3];
    LPC_EMAC->SA2 = (mac_address[4] << 8) | mac_address[5];

    /* Allocate the receive fragment buffers. */
    for (i = 0; i < DESC_LEN; i++) {
        rx_desc[i].packet = get_mem(RX_FRAG_BUF_SZ);
        rx_desc[i].control = (RX_FRAG_BUF_SZ - 1) | (1 << 31);
    }

    /* Copy to our static mac address variable. */
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        ether_addr[i] = mac_address[i];

    /* Set the txrx desc base address. */
    LPC_EMAC->RxDescriptor = (uint32_t)rx_desc;
    LPC_EMAC->TxDescriptor = (uint32_t)tx_desc;
    LPC_EMAC->TxStatus = (uint32_t)tx_status;
    LPC_EMAC->RxStatus = (uint32_t)rx_status;

    /* Set the txrx desc array length. */
    LPC_EMAC->RxDescriptorNumber = DESC_LEN - 1;
    LPC_EMAC->TxDescriptorNumber = DESC_LEN - 1;

    /* Zero the indices. */
    LPC_EMAC->RxConsumeIndex = 0;
    LPC_EMAC->TxProduceIndex = 0;

    /* Set RECEIVE_ENABLE */
    LPC_EMAC->MAC1 |= 1;

    /* Enable interrupts on rx. */
    LPC_EMAC->IntEnable |= (1 << 3);

    /* Enable Tx & Rx! */
    LPC_EMAC->Command |= 3 | (1 << 9) | (1 << 7);

    /* Enable Ethernet Mac interrupts. */
    LPC_NVIC->ISER0 = (1 << 28);
}
initcall(emac_init);

void emac_xmit_frame(ethernet_header *header,
                     void *payload, int payload_len)
{
    int desc_idx;

    /* Set the descriptor for the header. */
    desc_idx = LPC_EMAC->TxProduceIndex % DESC_LEN;
    tx_desc[desc_idx].packet = header;
    tx_desc[desc_idx].control = sizeof(*header) - 1;

    /* Set the descriptor for the payload packet. */
    desc_idx = (LPC_EMAC->TxProduceIndex + 1) % DESC_LEN;
    tx_desc[desc_idx].packet = payload;
    tx_desc[desc_idx].control = payload_len - 1;
    tx_desc[desc_idx].control |= (1 << 30); /* set the LAST bit. */

    /* Increment the TX produce index. */
    LPC_EMAC->TxProduceIndex = (desc_idx + 1) % DESC_LEN;

    /* Wait for xmit to complete. */
    while (LPC_EMAC->TxProduceIndex != LPC_EMAC->TxConsumeIndex) {};
}
