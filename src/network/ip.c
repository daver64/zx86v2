#include "network.h"
#include "stdio.h"
#include "string.h"
#include "kheap.h"

// Our network configuration
uint32_t our_ip_address = 0x0A00020F;    // 10.0.2.15 (QEMU default)
uint32_t our_netmask = 0xFFFFFF00;       // 255.255.255.0
uint32_t our_gateway = 0x0A000202;       // 10.0.2.2 (QEMU gateway)
uint8_t gateway_mac[6] = {0x52, 0x55, 0x00, 0x12, 0x34, 0x56}; // Default, updated by ARP

void ip_init(void) {
    extern void serial_puts(const char *msg);
    
    // IP stack initialization - no verbose output
}

uint16_t ip_checksum(uint8_t *data, uint32_t length) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t*)data;
    
    // Sum all 16-bit words
    while (length > 1) {
        sum += ntohs(*ptr++);
        length -= 2;
    }
    
    // Add odd byte if present
    if (length > 0) {
        sum += (*(uint8_t*)ptr) << 8;
    }
    
    // Add carry bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return htons(~sum);
}

void ip_send_packet(uint32_t dest_ip, uint8_t protocol, uint8_t *data, uint32_t data_length) {
    extern void serial_puts(const char *msg);
    
    uint32_t total_length = sizeof(ethernet_frame_t) + sizeof(ip_header_t) + data_length;
    uint8_t *packet = (uint8_t*)kmalloc(total_length);
    
    if (!packet) {
        serial_puts("SERIAL: NET - ERROR: IP send: out of memory\n");
        return;
    }
    
    // Build Ethernet frame
    ethernet_frame_t *eth = (ethernet_frame_t*)packet;
    
    // For simplicity, use broadcast MAC (in real implementation, use ARP)
    memset(eth->dest_mac, 0xFF, 6);  // Broadcast MAC
    
    // Get our MAC address from RTL8139 device
    memcpy(eth->src_mac, rtl8139_device.mac_addr, 6);
    
    eth->ethertype = htons(0x0800);  // IPv4
    
    // Build IP header
    ip_header_t *ip = (ip_header_t*)(packet + sizeof(ethernet_frame_t));
    ip->version_ihl = 0x45;  // IPv4, 20-byte header
    ip->tos = 0;
    ip->total_length = htons(sizeof(ip_header_t) + data_length);
    ip->identification = htons(12345);  // Should be unique per packet
    ip->flags_fragment = htons(0x4000);  // Don't fragment
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->src_ip = htonl(our_ip_address);
    ip->dest_ip = htonl(dest_ip);
    ip->checksum = 0;
    
    // Calculate IP checksum
    ip->checksum = ip_checksum((uint8_t*)ip, sizeof(ip_header_t));
    
    // Copy payload data
    memcpy(ip->data, data, data_length);
    
    // Send packet via RTL8139
    rtl8139_send_packet(packet, total_length);
    
    kfree(packet);
}

void ip_receive_packet(uint8_t *packet, uint32_t length) {
    extern void serial_puts(const char *msg);
    
    if (length < sizeof(ethernet_frame_t)) {
        serial_puts("SERIAL: NET - ERROR: packet too short for Ethernet header\n");
        return;
    }
    
    ethernet_frame_t *eth = (ethernet_frame_t*)packet;
    uint16_t ethertype = ntohs(eth->ethertype);
    
    if (ethertype == 0x0800) {
        // IPv4 packet
        if (length < sizeof(ethernet_frame_t) + sizeof(ip_header_t)) {
            serial_puts("SERIAL: NET - ERROR: IPv4 packet too short\n");
            return;
        }
        
        ip_header_t *ip = (ip_header_t*)(packet + sizeof(ethernet_frame_t));
        
        // Check if packet is for us or broadcast
        uint32_t dest_ip = ntohl(ip->dest_ip);
        if (dest_ip != our_ip_address && dest_ip != 0xFFFFFFFF) {
            return;  // Not for us, silently drop
        }
        
        // Verify IP checksum
        uint16_t received_checksum = ip->checksum;
        ip->checksum = 0;
        uint16_t calculated_checksum = ip_checksum((uint8_t*)ip, sizeof(ip_header_t));
        
        if (received_checksum != calculated_checksum) {
            serial_puts("SERIAL: NET - ERROR: IP checksum mismatch\n");
            return;
        }
        
        // Restore checksum
        ip->checksum = received_checksum;
        
        uint32_t src_ip = ntohl(ip->src_ip);
        uint32_t payload_length = ntohs(ip->total_length) - sizeof(ip_header_t);
        
        // Process based on protocol
        switch (ip->protocol) {
            case 1:  // ICMP
                icmp_receive_packet(ip->data, payload_length, src_ip);
                break;
            case 6:  // TCP
                tcp_receive_packet(ip, (tcp_header_t*)ip->data, payload_length);
                break;
            case 17: // UDP
                // UDP not implemented yet
                break;
            default:
                char debug_msg[64];
                sprintf(debug_msg, "SERIAL: NET - ERROR: unknown IP protocol %d\n", ip->protocol);
                serial_puts(debug_msg);
                break;
        }
    } else if (ethertype == 0x0806) {
        // ARP packet
        arp_receive_packet(packet + sizeof(ethernet_frame_t), 
                          length - sizeof(ethernet_frame_t));
    } else {
        char debug_msg[64];
        sprintf(debug_msg, "SERIAL: NET - ERROR: unknown ethertype: 0x%04X\n", ethertype);
        serial_puts(debug_msg);
    }
}

void arp_init(void) {
    extern void serial_puts(const char *msg);
    
    // ARP service initialization - no verbose output
}

void arp_receive_packet(uint8_t *packet, uint32_t length) {
    extern void serial_puts(const char *msg);
    
    if (length < sizeof(arp_header_t)) {
        serial_puts("SERIAL: NET - ERROR: ARP packet too short\n");
        return;
    }
    
    arp_header_t *arp = (arp_header_t*)packet;
    
    // Check if it's Ethernet/IPv4 ARP
    if (ntohs(arp->hardware_type) != ARP_HARDWARE_ETHERNET ||
        ntohs(arp->protocol_type) != ARP_PROTOCOL_IPV4) {
        serial_puts("SERIAL: NET - ERROR: unsupported ARP type\n");
        return;
    }
    
    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);
    uint16_t operation = ntohs(arp->operation);
    
    if (operation == ARP_REQUEST && target_ip == our_ip_address) {
        printf("ARP: Request for our IP %d.%d.%d.%d - sending reply\n",
               (our_ip_address >> 24) & 0xFF, (our_ip_address >> 16) & 0xFF,
               (our_ip_address >> 8) & 0xFF, our_ip_address & 0xFF);
        
        // If this is from our gateway, store its MAC address
        if (sender_ip == our_gateway) {
            memcpy(gateway_mac, arp->sender_mac, 6);
            printf("ARP: Updated gateway MAC to %02X:%02X:%02X:%02X:%02X:%02X\n",
                   gateway_mac[0], gateway_mac[1], gateway_mac[2],
                   gateway_mac[3], gateway_mac[4], gateway_mac[5]);
        }
        
        // Send ARP reply
        arp_send_reply(sender_ip, arp->sender_mac);
    }
}

void arp_send_reply(uint32_t dest_ip, uint8_t dest_mac[6]) {
    extern void serial_puts(const char *msg);
    
    uint32_t total_length = sizeof(ethernet_frame_t) + sizeof(arp_header_t);
    uint8_t *packet = (uint8_t*)kmalloc(total_length);
    
    if (!packet) {
        serial_puts("SERIAL: NET - ERROR: ARP reply: out of memory\n");
        return;
    }
    
    // Build Ethernet frame
    ethernet_frame_t *eth = (ethernet_frame_t*)packet;
    memcpy(eth->dest_mac, dest_mac, 6);
    memcpy(eth->src_mac, rtl8139_device.mac_addr, 6);
    eth->ethertype = htons(0x0806);  // ARP
    
    // Build ARP reply
    arp_header_t *arp = (arp_header_t*)(packet + sizeof(ethernet_frame_t));
    arp->hardware_type = htons(ARP_HARDWARE_ETHERNET);
    arp->protocol_type = htons(ARP_PROTOCOL_IPV4);
    arp->hardware_length = 6;
    arp->protocol_length = 4;
    arp->operation = htons(ARP_REPLY);
    
    // Fill in addresses
    memcpy(arp->sender_mac, rtl8139_device.mac_addr, 6);
    arp->sender_ip = htonl(our_ip_address);
    memcpy(arp->target_mac, dest_mac, 6);
    arp->target_ip = htonl(dest_ip);
    
    // Send the packet
    rtl8139_send_packet(packet, total_length);
    
    kfree(packet);
}