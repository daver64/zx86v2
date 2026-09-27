#include "network.h"
#include "common.h"
#include "string.h"
#include "stdio.h"

// External declarations
extern uint32_t our_ip_address;

// Simple echo server state
#define MAX_CONNECTIONS 8
#define ECHO_SERVER_PORT 7

typedef struct {
    uint32_t remote_ip;
    uint16_t remote_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t state; // 0=closed, 1=syn_received, 2=established
} tcp_connection_t;

static tcp_connection_t connections[MAX_CONNECTIONS];
static bool tcp_initialized = false;

void tcp_init(void) {
    // Initialize connection array
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connections[i].state = 0;
        connections[i].remote_ip = 0;
        connections[i].remote_port = 0;
        connections[i].sequence = 0;
        connections[i].acknowledgment = 0;
    }
    tcp_initialized = true;
    // TCP echo server initialization - no verbose output
}

uint16_t tcp_checksum(ip_header_t* ip_hdr, tcp_header_t* tcp_hdr, uint16_t tcp_len) {
    uint32_t sum = 0;
    
    // Save original checksum and set to 0 for calculation
    uint16_t orig_checksum = tcp_hdr->checksum;
    tcp_hdr->checksum = 0;
    
    // Add pseudo-header: src_ip + dest_ip + protocol + length
    // IP addresses are in network byte order, convert for checksum
    uint32_t src_ip = ntohl(ip_hdr->src_ip);
    uint32_t dest_ip = ntohl(ip_hdr->dest_ip);
    
    sum += (src_ip >> 16) + (src_ip & 0xFFFF);
    sum += (dest_ip >> 16) + (dest_ip & 0xFFFF);
    sum += 6; // TCP protocol
    sum += tcp_len;
    
    // Add TCP header and data
    uint8_t* data = (uint8_t*)tcp_hdr;
    for (int i = 0; i < tcp_len; i += 2) {
        if (i + 1 < tcp_len) {
            sum += (data[i] << 8) | data[i + 1];
        } else {
            sum += data[i] << 8;
        }
    }
    
    // Fold carry bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    // Restore original checksum
    tcp_hdr->checksum = orig_checksum;
    
    return ~sum;
}

tcp_connection_t* find_connection(uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].state != 0 && 
            connections[i].remote_ip == remote_ip && 
            connections[i].remote_port == remote_port) {
            return &connections[i];
        }
    }
    return NULL;
}

tcp_connection_t* new_connection(uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i].state == 0) {
            connections[i].remote_ip = remote_ip;
            connections[i].remote_port = remote_port;
            connections[i].sequence = 1000; // Initial sequence number
            connections[i].acknowledgment = 0;
            connections[i].state = 1; // SYN_RECEIVED
            return &connections[i];
        }
    }
    return NULL; // No free connections
}

void tcp_send_packet(uint32_t dest_ip, uint16_t dest_port, uint16_t src_port,
                     uint32_t seq, uint32_t ack, uint8_t flags, 
                     const uint8_t* data, uint16_t data_len) {
    
    // Use static buffer to avoid stack overflow
    static uint8_t packet[1500];
    ethernet_frame_t* eth = (ethernet_frame_t*)packet;
    ip_header_t* ip = (ip_header_t*)(packet + sizeof(ethernet_frame_t));
    tcp_header_t* tcp = (tcp_header_t*)(packet + sizeof(ethernet_frame_t) + sizeof(ip_header_t));
    
    // Ethernet header
    extern uint8_t gateway_mac[6];
    memcpy(eth->dest_mac, gateway_mac, 6); // Use learned gateway MAC
    memcpy(eth->src_mac, rtl8139_device.mac_addr, 6);
    eth->ethertype = htons(0x0800); // IP
    
    // IP header
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(sizeof(ip_header_t) + sizeof(tcp_header_t) + data_len);
    ip->identification = htons(0x1234);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = 6; // TCP
    ip->checksum = 0;
    ip->src_ip = htonl(our_ip_address);
    ip->dest_ip = htonl(dest_ip);
    
    // Calculate IP checksum
    uint32_t ip_sum = 0;
    uint8_t* ip_bytes = (uint8_t*)ip;
    for (int i = 0; i < 20; i += 2) {
        ip_sum += (ip_bytes[i] << 8) | ip_bytes[i + 1];
    }
    while (ip_sum >> 16) {
        ip_sum = (ip_sum & 0xFFFF) + (ip_sum >> 16);
    }
    ip->checksum = htons(~ip_sum);
    
    // TCP header
    tcp->src_port = htons(src_port);
    tcp->dest_port = htons(dest_port);
    tcp->sequence = htonl(seq);
    tcp->acknowledgment = htonl(ack);
    tcp->data_offset_flags = 0x50; // 20-byte header, no options
    tcp->flags = flags;
    tcp->window_size = htons(8192);
    tcp->checksum = 0;
    tcp->urgent_pointer = 0;
    
    // Copy data
    if (data && data_len > 0) {
        memcpy(tcp->data, data, data_len);
    }
    
    // Calculate TCP checksum
    uint16_t tcp_len = sizeof(tcp_header_t) + data_len;
    tcp->checksum = htons(tcp_checksum(ip, tcp, tcp_len));
    
    uint16_t total_len = sizeof(ethernet_frame_t) + sizeof(ip_header_t) + tcp_len;
    rtl8139_send_packet(packet, total_len);
}

void tcp_receive_packet(ip_header_t* ip_hdr, tcp_header_t* tcp_hdr, uint16_t tcp_len) {
    extern void serial_puts(const char *msg);
    char debug_msg[128];
    
    if (!tcp_initialized) {
        serial_puts("SERIAL: NET - ERROR: TCP not initialized, ignoring packet\n");
        return;
    }
    
    if (tcp_len < sizeof(tcp_header_t)) {
        serial_puts("SERIAL: NET - ERROR: TCP packet too small for header\n");
        return;
    }
    
    uint32_t src_ip = ntohl(ip_hdr->src_ip);
    uint16_t src_port = ntohs(tcp_hdr->src_port);
    uint16_t dest_port = ntohs(tcp_hdr->dest_port);
    uint32_t seq = ntohl(tcp_hdr->sequence);
    uint32_t ack = ntohl(tcp_hdr->acknowledgment);
    uint8_t flags = tcp_hdr->flags;
    
    // Only handle echo server port for now
    if (dest_port != ECHO_SERVER_PORT) {
        tcp_send_packet(src_ip, src_port, dest_port, 0, seq + 1, TCP_FLAG_RST, NULL, 0);
        return;
    }
    
    tcp_connection_t* conn = find_connection(src_ip, src_port);
    
    if (flags & TCP_FLAG_SYN) {
        if (conn == NULL) {
            // New connection
            conn = new_connection(src_ip, src_port);
            if (conn == NULL) {
                serial_puts("SERIAL: NET - ERROR: [TCP] No free connections, sending RST\n");
                tcp_send_packet(src_ip, src_port, dest_port, 0, seq + 1, TCP_FLAG_RST, NULL, 0);
                return;
            }
            conn->acknowledgment = seq + 1;
            
            // Send SYN+ACK
            printf("TCP: New connection from %d.%d.%d.%d:%d\n", 
                   (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                   (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port);
            tcp_send_packet(src_ip, src_port, dest_port, conn->sequence, 
                          conn->acknowledgment, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
            conn->sequence++;
        }
    } else if (flags & TCP_FLAG_ACK && conn && conn->state == 1) {
        // Connection established
        printf("TCP: Connection established with %d.%d.%d.%d:%d\n", 
               (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
               (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port);
        conn->state = 2;
        conn->acknowledgment = seq;
    } else if (flags & TCP_FLAG_FIN && conn) {
        // Close connection
        printf("TCP: Closing connection with %d.%d.%d.%d:%d\n", 
               (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
               (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port);
        conn->acknowledgment = seq + 1;
        tcp_send_packet(src_ip, src_port, dest_port, conn->sequence, 
                      conn->acknowledgment, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        conn->state = 0; // Close connection
    } else if (conn && conn->state == 2) {
        // Handle data in established connection
        uint16_t data_len = tcp_len - sizeof(tcp_header_t);
        if (data_len > 0 && data_len < 1400) { // Reasonable size limit
            // Echo the data back
            conn->acknowledgment = seq + data_len;
            tcp_send_packet(src_ip, src_port, dest_port, conn->sequence, 
                          conn->acknowledgment, TCP_FLAG_PSH | TCP_FLAG_ACK, 
                          tcp_hdr->data, data_len);
            conn->sequence += data_len;
        } else if (flags & TCP_FLAG_ACK) {
            // Just an ACK, update our state
            conn->acknowledgment = seq;
        }
    }
}