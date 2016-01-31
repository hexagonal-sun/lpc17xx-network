#include "LPC17xx.h"
#include "arp.h"
#include "memory.h"
#include "byteswap.h"

#define DESC_LEN 5

typedef struct
{
    void *packet;
    uint32_t control;
} txrx_descriptor;

/* txrx descriptor arrays. */
txrx_descriptor tx_desc[DESC_LEN];
txrx_descriptor rx_desc[DESC_LEN];
uint32_t tx_status[DESC_LEN];

/* Our ethernet address, set at init time. */
uint8_t ether_addr[ETHER_ADDR_LEN];

#define PHY_ADDR 1

void phy_write(int reg, int writeval)
{
    LPC_EMAC->MCMD = 0;
    LPC_EMAC->MADR = reg | (PHY_ADDR << 8);
    LPC_EMAC->MWTD = writeval;
    while (LPC_EMAC->MIND & 0x1) {};
}

unsigned short phy_read(unsigned char reg)
{
    LPC_EMAC->MCMD = 0x1;
    LPC_EMAC->MADR = reg | (PHY_ADDR << 8);
    while  (LPC_EMAC->MIND & 0x1) {};
    LPC_EMAC->MCMD = 0;
    return (LPC_EMAC->MRDD);
}

void ethernet_init(uint8_t mac_address[6])
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

    /* Copy to our static mac address variable. */
    for (i = 0; i < ETHER_ADDR_LEN; i++)
        ether_addr[i] = mac_address[i];

    /* Set the txrx desc base address. */
    LPC_EMAC->RxDescriptor = (uint32_t)rx_desc;
    LPC_EMAC->TxDescriptor = (uint32_t)tx_desc;
    LPC_EMAC->TxStatus = (uint32_t)tx_status;

    /* Set the txrx desc array length. */
    LPC_EMAC->RxDescriptorNumber = DESC_LEN - 1;
    LPC_EMAC->TxDescriptorNumber = DESC_LEN - 1;

    /* Zero the indices. */
    LPC_EMAC->RxConsumeIndex = 0;
    LPC_EMAC->TxProduceIndex = 0;

    /* Enable Tx! */
    LPC_EMAC->Command |= 2 | (1 << 9);

}

static void emac_xmit_frame(ethernet_header *header, void *payload, int payload_len)
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

void ether_xmit_payload(uint8_t dhost[ETHER_ADDR_LEN], uint16_t ether_type,
                        void *payload, int len)
{
    int i;
    ethernet_header *header = (ethernet_header *)get_mem(sizeof(ethernet_header));

    for (i = 0; i < ETHER_ADDR_LEN; i++) {
        header->ether_dhost[i] = dhost[i];
        header->ether_shost[i] = ether_addr[i];
    }

    header->ether_type = ether_type;
    swap_endian16(&header->ether_type);

    emac_xmit_frame(header, payload, len);

    free_mem(header);
}
