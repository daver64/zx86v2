#include "network.h"
#include "stdio.h"
#include "string.h"
#include "kheap.h"

void icmp_init(void) {
    extern void serial_puts(const char *msg);
    
    // ICMP (ping) service initialization - no verbose output
    serial_puts("SERIAL: NET - ICMP initialized\n");
}

void icmp_send_reply(uint32_t dest_ip, uint16_t identifier, uint16_t sequence, 
                     uint8_t *data, uint32_t data_length) {
    extern void serial_puts(const char *msg);
    
    uint32_t total_length = sizeof(icmp_header_t) + data_length;
    uint8_t *icmp_packet = (uint8_t*)kmalloc(total_length);
    
    if (!icmp_packet) {
        serial_puts("SERIAL: NET - ICMP reply: out of memory\n");
        return;
    }
    
    // Build ICMP reply
    icmp_header_t *icmp = (icmp_header_t*)icmp_packet;
    icmp->type = ICMP_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = identifier;  // Echo back the same identifier
    icmp->sequence = sequence;      // Echo back the same sequence
    
    // Copy ping data back exactly as received
    if (data_length > 0) {
        memcpy(icmp->data, data, data_length);
    }
    
    // Calculate ICMP checksum
    icmp->checksum = ip_checksum(icmp_packet, total_length);
    
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: NET - ICMP reply to %d.%d.%d.%d: id=%d, seq=%d, %d bytes\n",
            (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
            (dest_ip >> 8) & 0xFF, dest_ip & 0xFF,
            ntohs(identifier), ntohs(sequence), data_length);
    serial_puts(debug_msg);
    
    // Send via IP layer (protocol 1 = ICMP)
    ip_send_packet(dest_ip, 1, icmp_packet, total_length);
    
    // Print to console
    printf("PING: Replied to %d.%d.%d.%d (id=%d, seq=%d, %d bytes)\n",
           (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
           (dest_ip >> 8) & 0xFF, dest_ip & 0xFF,
           ntohs(identifier), ntohs(sequence), data_length);
    
    kfree(icmp_packet);
}

void icmp_receive_packet(uint8_t *packet, uint32_t length, uint32_t src_ip) {
    extern void serial_puts(const char *msg);
    
    if (length < sizeof(icmp_header_t)) {
        serial_puts("SERIAL: NET - ICMP packet too short\n");
        return;
    }
    
    icmp_header_t *icmp = (icmp_header_t*)packet;
    
    // Verify ICMP checksum
    uint16_t received_checksum = icmp->checksum;
    icmp->checksum = 0;
    uint16_t calculated_checksum = ip_checksum(packet, length);
    
    if (received_checksum != calculated_checksum) {
        serial_puts("SERIAL: NET - ICMP checksum mismatch\n");
        return;
    }
    
    // Restore checksum
    icmp->checksum = received_checksum;
    
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: NET - ICMP receive: type %d, code %d from %d.%d.%d.%d\n",
            icmp->type, icmp->code,
            (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
            (src_ip >> 8) & 0xFF, src_ip & 0xFF);
    serial_puts(debug_msg);
    
    switch (icmp->type) {
        case ICMP_ECHO_REQUEST:
            {
                // Ping request - send reply
                uint32_t data_length = length - sizeof(icmp_header_t);
                
                printf("PING: Request from %d.%d.%d.%d (id=%d, seq=%d, %d bytes)\n",
                       (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                       (src_ip >> 8) & 0xFF, src_ip & 0xFF,
                       ntohs(icmp->identifier), ntohs(icmp->sequence), data_length);
                
                // Send reply with same identifier, sequence, and data
                icmp_send_reply(src_ip, icmp->identifier, icmp->sequence, 
                               icmp->data, data_length);
            }
            break;
            
        case ICMP_ECHO_REPLY:
            printf("PING: Reply from %d.%d.%d.%d (id=%d, seq=%d)\n",
                   (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                   (src_ip >> 8) & 0xFF, src_ip & 0xFF,
                   ntohs(icmp->identifier), ntohs(icmp->sequence));
            break;
            
        case ICMP_DEST_UNREACH:
            printf("PING: Destination unreachable from %d.%d.%d.%d\n",
                   (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                   (src_ip >> 8) & 0xFF, src_ip & 0xFF);
            break;
            
        default:
            sprintf(debug_msg, "SERIAL: NET - unknown ICMP type %d\n", icmp->type);
            serial_puts(debug_msg);
            break;
    }
}

void icmp_send_request(uint32_t dest_ip, uint16_t identifier, uint16_t sequence, 
                       uint8_t *data, uint32_t data_length) {
    extern void serial_puts(const char *msg);
    
    uint32_t total_length = sizeof(icmp_header_t) + data_length;
    uint8_t *icmp_packet = (uint8_t*)kmalloc(total_length);
    
    if (!icmp_packet) {
        serial_puts("SERIAL: NET - ICMP request: out of memory\n");
        return;
    }
    
    // Build ICMP echo request
    icmp_header_t *icmp = (icmp_header_t*)icmp_packet;
    icmp->type = ICMP_ECHO_REQUEST;  // Type 8 for echo request
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->identifier = identifier;
    icmp->sequence = sequence;
    
    // Copy data
    if (data && data_length > 0) {
        memcpy(icmp->data, data, data_length);
    }
    
    // Calculate checksum
    icmp->checksum = ip_checksum((uint8_t*)icmp, total_length);
    
    char debug_msg[128];
    sprintf(debug_msg, "SERIAL: NET - ICMP request to %d.%d.%d.%d: id=%d, seq=%d, %d bytes\n",
            (dest_ip >> 24) & 0xFF, (dest_ip >> 16) & 0xFF,
            (dest_ip >> 8) & 0xFF, dest_ip & 0xFF,
            identifier, sequence, data_length);
    serial_puts(debug_msg);
    
    // Send via IP layer (protocol 1 = ICMP)
    ip_send_packet(dest_ip, 1, icmp_packet, total_length);
    
    kfree(icmp_packet);
}