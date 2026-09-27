#ifndef NETWORK_H
#define NETWORK_H

#include "common.h"
#include "pci.h"

// Forward declaration
typedef struct registers registers_t;

// RTL8139 Network Card Definitions
#define RTL8139_VENDOR_ID    0x10EC
#define RTL8139_DEVICE_ID    0x8139

// RTL8139 register offsets
#define RTL8139_IDR0         0x00    // MAC address (6 bytes)
#define RTL8139_MAR0         0x08    // Multicast filter
#define RTL8139_TXSTATUS0    0x10    // TX status (4 32bit regs)
#define RTL8139_TXADDR0      0x20    // TX address (4 32bit regs)
#define RTL8139_RXBUF        0x30    // RX buffer start address
#define RTL8139_CMD          0x37    // Command register
#define RTL8139_RXBUFTAIL    0x38    // RX buffer tail
#define RTL8139_RXBUFHEAD    0x3A    // RX buffer head
#define RTL8139_IMR          0x3C    // Interrupt mask
#define RTL8139_ISR          0x3E    // Interrupt status
#define RTL8139_TCR          0x40    // TX config
#define RTL8139_RCR          0x44    // RX config
#define RTL8139_CONFIG1      0x52    // Configuration register 1

// Command register bits
#define RTL8139_CMD_RESET    0x10
#define RTL8139_CMD_RX_EN    0x08
#define RTL8139_CMD_TX_EN    0x04

// Interrupt bits
#define RTL8139_INT_RX_OK    0x01
#define RTL8139_INT_RX_ERR   0x02
#define RTL8139_INT_TX_OK    0x04
#define RTL8139_INT_TX_ERR   0x08
#define RTL8139_INT_LINK     0x20

// Buffer sizes
#define RTL8139_RX_BUF_SIZE  (8192 + 16 + 1500) // 8KB + padding for DMA
#define RTL8139_TX_BUF_SIZE  1536

// Network packet structures
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

typedef struct {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t data[];
} __attribute__((packed)) ip_header_t;

// ICMP Protocol definitions
#define ICMP_ECHO_REQUEST    8
#define ICMP_ECHO_REPLY      0
#define ICMP_DEST_UNREACH    3
#define ICMP_TIME_EXCEEDED   11

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
    uint8_t data[];
} __attribute__((packed)) icmp_header_t;

// TCP Protocol definitions
#define TCP_FLAG_FIN     0x01
#define TCP_FLAG_SYN     0x02
#define TCP_FLAG_RST     0x04
#define TCP_FLAG_PSH     0x08
#define TCP_FLAG_ACK     0x10
#define TCP_FLAG_URG     0x20

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t data_offset_flags; // Upper 4 bits: data offset, lower 4: reserved
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
    uint8_t data[];
} __attribute__((packed)) tcp_header_t;

// ARP Protocol definitions
#define ARP_HARDWARE_ETHERNET    1
#define ARP_PROTOCOL_IPV4        0x0800
#define ARP_REQUEST              1
#define ARP_REPLY                2

typedef struct {
    uint16_t hardware_type;      // 1 for Ethernet
    uint16_t protocol_type;      // 0x0800 for IPv4
    uint8_t hardware_length;     // 6 for Ethernet
    uint8_t protocol_length;     // 4 for IPv4
    uint16_t operation;          // 1 = request, 2 = reply
    uint8_t sender_mac[6];       // Sender hardware address
    uint32_t sender_ip;          // Sender protocol address
    uint8_t target_mac[6];       // Target hardware address
    uint32_t target_ip;          // Target protocol address
} __attribute__((packed)) arp_header_t;

// RTL8139 device structure
typedef struct {
    uint16_t io_base;
    uint8_t irq;
    uint8_t mac_addr[6];
    uint32_t rx_buffer_phys;
    uint8_t *rx_buffer;
    uint32_t rx_buffer_pos;
    uint32_t tx_buffer_phys[4];
    uint8_t *tx_buffer[4];
    uint8_t tx_current;
    bool initialized;
    
    // Statistics
    uint32_t packets_received;
    uint32_t packets_sent;
    uint32_t bytes_received;
    uint32_t bytes_sent;
    uint32_t rx_errors;
    uint32_t tx_errors;
} rtl8139_dev_t;

// Network work item for AMP processing
typedef struct {
    uint32_t work_type;
    void *data_ptr;
    uint32_t data_size;
    uint32_t timestamp;
} network_work_t;

#define NETWORK_WORK_RX_PACKET    1
#define NETWORK_WORK_TX_COMPLETE  2
#define NETWORK_WORK_LINK_STATUS  3

// RTL8139 device instance
extern rtl8139_dev_t rtl8139_device;

// Function declarations
bool rtl8139_detect(void);
bool rtl8139_init(void);
void rtl8139_send_packet(uint8_t *data, uint32_t length);
void rtl8139_interrupt_handler(registers_t *regs);
void rtl8139_print_stats(void);

// Shell command interface functions
void rtl8139_show_status(void);
void rtl8139_show_stats(void);
int rtl8139_send_test_packet(void);

// Network processing functions (for AMP)
void* network_process_rx(void *args);
void* network_process_tx_complete(void *args);

// Network stack functions
void ip_init(void);
void icmp_init(void);
void arp_init(void);
void tcp_init(void);
void ip_receive_packet(uint8_t *packet, uint32_t length);
void ip_send_packet(uint32_t dest_ip, uint8_t protocol, uint8_t *data, uint32_t data_length);
uint16_t ip_checksum(uint8_t *data, uint32_t length);
void tcp_receive_packet(ip_header_t* ip_hdr, tcp_header_t* tcp_hdr, uint16_t tcp_len);
void icmp_receive_packet(uint8_t *packet, uint32_t length, uint32_t src_ip);
void arp_receive_packet(uint8_t *packet, uint32_t length);
void arp_send_reply(uint32_t dest_ip, uint8_t dest_mac[6]);

// Network configuration
extern uint8_t gateway_mac[6];

// Utility functions
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);

#endif