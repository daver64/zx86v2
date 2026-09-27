#include "network.h"
#include "pci.h"
#include "paging.h"
#include "amp.h"
#include "stdio.h"
#include "string.h"
#include "kheap.h"

// RTL8139 device instance - global so other network modules can access it
rtl8139_dev_t rtl8139_device;

// Network byte order functions
uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | 
           (((hostlong >> 8) & 0xFF) << 16) |
           (((hostlong >> 16) & 0xFF) << 8) |
           ((hostlong >> 24) & 0xFF);
}

uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);  // Same operation
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);   // Same operation
}

bool rtl8139_detect(void) {
    extern void serial_puts(const char *msg);
    
    // Scan PCI bus for RTL8139
    pci_device_t *pci_dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    
    if (pci_dev) {
        printf("NET: RTL8139 network card detected!\n");
        printf("NET: Location: %02X:%02X.%X\n", 
               pci_dev->bus, pci_dev->device, pci_dev->function);
        printf("NET: Vendor ID: 0x%04X, Device ID: 0x%04X\n",
               pci_dev->vendor_id, pci_dev->device_id);
        return true;
    } else {
        printf("NET: ERROR - No RTL8139 network card found\n");
        return false;
    }
}

bool rtl8139_init(void) {
    extern void serial_puts(const char *msg);
    
    // First detect the card
    if (!rtl8139_detect()) {
        return false;
    }
    
    printf("NET: Initializing RTL8139 network card...\n");
    
    // Find RTL8139 PCI device
    pci_device_t *pci_dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!pci_dev) {
        printf("NET: ERROR - RTL8139 disappeared during init\n");
        return false;
    }
    
    // Get I/O base address from BAR0
    rtl8139_device.io_base = pci_dev->bar[0] & 0xFFFC;  // Clear bottom 2 bits
    
    // Read interrupt line from PCI config space (offset 0x3C)
    uint32_t interrupt_config = pci_read_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x3C);
    rtl8139_device.irq = interrupt_config & 0xFF;  // Interrupt line is in bits 7:0
    
    printf("NET: I/O base address: 0x%04X\n", rtl8139_device.io_base);
    printf("NET: IRQ line: %d\n", rtl8139_device.irq);
    
    if (rtl8139_device.io_base == 0) {
        printf("NET: ERROR - Invalid I/O base address\n");
        return false;
    }
    
    // Enable PCI device (I/O space and bus mastering)
    uint32_t command_reg = pci_read_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x04);
    uint16_t command = command_reg & 0xFFFF;
    command |= 0x05;  // Enable I/O space and bus mastering
    pci_write_config(pci_dev->bus, pci_dev->device, pci_dev->function, 0x04, 
                    (command_reg & 0xFFFF0000) | command);
    
    // Software reset
    printf("NET: Performing software reset...\n");
    outb(rtl8139_device.io_base + RTL8139_CMD, RTL8139_CMD_RESET);
    
    // Wait for reset to complete (timeout after ~100ms)
    int reset_timeout = 1000000;
    while ((inb(rtl8139_device.io_base + RTL8139_CMD) & RTL8139_CMD_RESET) && reset_timeout > 0) {
        reset_timeout--;
    }
    
    if (reset_timeout == 0) {
        printf("NET: ERROR - Reset timeout\n");
        return false;
    }
    
    // Allocate RX buffer (must be physically contiguous)
    rtl8139_device.rx_buffer = (uint8_t*)kmalloc_ap(RTL8139_RX_BUF_SIZE, 
                                                    &rtl8139_device.rx_buffer_phys);
    if (!rtl8139_device.rx_buffer) {
        printf("NET: ERROR - Failed to allocate RX buffer\n");
        return false;
    }
    
    // Clear RX buffer
    memset(rtl8139_device.rx_buffer, 0, RTL8139_RX_BUF_SIZE);
    
    printf("NET: RX buffer allocated at 0x%08X (physical 0x%08X)\n",
           (uint32_t)rtl8139_device.rx_buffer, rtl8139_device.rx_buffer_phys);
    
    // Allocate TX buffers (4 separate buffers)
    for (int i = 0; i < 4; i++) {
        rtl8139_device.tx_buffer[i] = (uint8_t*)kmalloc_ap(RTL8139_TX_BUF_SIZE,
                                                           &rtl8139_device.tx_buffer_phys[i]);
        if (!rtl8139_device.tx_buffer[i]) {
            printf("NET: ERROR - Failed to allocate TX buffer %d\n", i);
            return false;
        }
        
        // Clear TX buffer
        memset(rtl8139_device.tx_buffer[i], 0, RTL8139_TX_BUF_SIZE);
    }
    
    // Set RX buffer address
    outl(rtl8139_device.io_base + RTL8139_RXBUF, rtl8139_device.rx_buffer_phys);
    
    // Initialize buffer positions
    rtl8139_device.rx_buffer_pos = 0;
    rtl8139_device.tx_current = 0;
    
    // Read MAC address from device
    for (int i = 0; i < 6; i++) {
        rtl8139_device.mac_addr[i] = inb(rtl8139_device.io_base + RTL8139_IDR0 + i);
    }
    
    printf("NET: MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           rtl8139_device.mac_addr[0], rtl8139_device.mac_addr[1],
           rtl8139_device.mac_addr[2], rtl8139_device.mac_addr[3],
           rtl8139_device.mac_addr[4], rtl8139_device.mac_addr[5]);
    
    // Configure RX: accept broadcast, multicast, and unicast packets
    // Bit 0: Accept All Packets (promiscuous mode - disabled)
    // Bit 1: Accept Physical Match
    // Bit 2: Accept Multicast
    // Bit 3: Accept Broadcast
    uint32_t rx_config = 0x0000000E;  // Accept multicast, broadcast, unicast
    outl(rtl8139_device.io_base + RTL8139_RCR, rx_config);
    
    // Configure TX: default settings with DMA burst
    uint32_t tx_config = 0x03000000;  // Default TX config
    outl(rtl8139_device.io_base + RTL8139_TCR, tx_config);
    
    // Enable interrupts (RX OK, TX OK, RX Error, TX Error)
    uint16_t interrupt_mask = RTL8139_INT_RX_OK | RTL8139_INT_TX_OK | 
                             RTL8139_INT_RX_ERR | RTL8139_INT_TX_ERR;
    outw(rtl8139_device.io_base + RTL8139_IMR, interrupt_mask);
    
    // Register interrupt handler
    if (rtl8139_device.irq < 16) {
        register_interrupt_handler(32 + rtl8139_device.irq, rtl8139_interrupt_handler);
        printf("NET: Interrupt handler registered for IRQ %d\n", rtl8139_device.irq);
    } else {
        printf("NET: WARNING - Invalid IRQ %d\n", rtl8139_device.irq);
    }
    
    // Enable RX and TX
    outb(rtl8139_device.io_base + RTL8139_CMD, 
         RTL8139_CMD_RX_EN | RTL8139_CMD_TX_EN);
    
    // Initialize statistics
    rtl8139_device.packets_received = 0;
    rtl8139_device.packets_sent = 0;
    rtl8139_device.bytes_received = 0;
    rtl8139_device.bytes_sent = 0;
    rtl8139_device.rx_errors = 0;
    rtl8139_device.tx_errors = 0;
    
    rtl8139_device.initialized = true;
    
    printf("NET: RTL8139 initialization complete!\n");
    
    return true;
}

void rtl8139_send_packet(uint8_t *data, uint32_t length) {
    extern void serial_puts(const char *msg);
    
    if (!rtl8139_device.initialized) {
        serial_puts("SERIAL: NET - ERROR: send failed: not initialized\n");
        return;
    }
    
    if (length > RTL8139_TX_BUF_SIZE) {
        printf("NET: ERROR - Packet too large (%d bytes, max %d)\n", length, RTL8139_TX_BUF_SIZE);
        rtl8139_device.tx_errors++;
        return;
    }
    
    // Copy data to current TX buffer
    memcpy(rtl8139_device.tx_buffer[rtl8139_device.tx_current], data, length);
    
    // Set TX descriptor registers
    uint32_t tx_addr_reg = RTL8139_TXADDR0 + (rtl8139_device.tx_current * 4);
    uint32_t tx_status_reg = RTL8139_TXSTATUS0 + (rtl8139_device.tx_current * 4);
    
    // Set physical address of TX buffer
    outl(rtl8139_device.io_base + tx_addr_reg, 
         rtl8139_device.tx_buffer_phys[rtl8139_device.tx_current]);
    
    // Set length and start transmission (length in bits 0-12, other bits are flags)
    outl(rtl8139_device.io_base + tx_status_reg, length);
    
    // Update statistics
    rtl8139_device.packets_sent++;
    rtl8139_device.bytes_sent += length;
    
    // Advance to next TX buffer
    rtl8139_device.tx_current = (rtl8139_device.tx_current + 1) % 4;
}

void rtl8139_interrupt_handler(registers_t *regs) {
    extern void serial_puts(const char *msg);
    
    // Suppress unused parameter warning
    (void)regs;
    
    if (!rtl8139_device.initialized) {
        return;
    }
    
    // Read interrupt status
    uint16_t status = inw(rtl8139_device.io_base + RTL8139_ISR);
    
    // Clear interrupts by writing back the status
    outw(rtl8139_device.io_base + RTL8139_ISR, status);
    
    if (status & RTL8139_INT_RX_OK) {
        // Forward packet processing to network CPU via AMP
        if (amp_is_initialized()) {
            network_work_t work = {
                .work_type = NETWORK_WORK_RX_PACKET,
                .data_ptr = &rtl8139_device,
                .data_size = 0,
                .timestamp = amp_get_timestamp()
            };
            
            // Execute on network CPU (we'll determine which CPU later)
            amp_execute_function_async(network_process_rx, &work, sizeof(work));
        } else {
            // Fallback: process on current CPU
            serial_puts("SERIAL: NET - ERROR: AMP not available, processing locally\n");
            network_work_t work = {NETWORK_WORK_RX_PACKET, &rtl8139_device, 0, 0};
            network_process_rx(&work);
        }
    }
    
    if (status & RTL8139_INT_TX_OK) {
        // TX completion can be handled quickly
        if (amp_is_initialized()) {
            network_work_t work = {
                .work_type = NETWORK_WORK_TX_COMPLETE,
                .data_ptr = &rtl8139_device,
                .data_size = 0,
                .timestamp = amp_get_timestamp()
            };
            
            amp_execute_function_async(network_process_tx_complete, &work, sizeof(work));
        }
    }
    
    if (status & RTL8139_INT_RX_ERR) {
        serial_puts("SERIAL: NET - ERROR: RX error\n");
        rtl8139_device.rx_errors++;
    }
    
    if (status & RTL8139_INT_TX_ERR) {
        serial_puts("SERIAL: NET - ERROR: TX error\n");
        rtl8139_device.tx_errors++;
    }
}

void* network_process_rx(void *args) {
    extern void serial_puts(const char *msg);
    network_work_t *work = (network_work_t*)args;
    
    if (!rtl8139_device.initialized) {
        return NULL;
    }
    
    // Process received packets
    uint16_t rx_head = inw(rtl8139_device.io_base + RTL8139_RXBUFHEAD);
    uint32_t rx_tail = rtl8139_device.rx_buffer_pos;
    
    while (rx_tail != rx_head) {
        // Bounds check for buffer wraparound
        if (rx_tail >= RTL8139_RX_BUF_SIZE) {
            serial_puts("SERIAL: NET - ERROR: RX tail out of bounds, resetting\n");
            rx_tail = 0;
            rtl8139_device.rx_buffer_pos = 0;
            break;
        }
        
        // Read packet header (4 bytes: status + length)
        uint32_t *header = (uint32_t*)(rtl8139_device.rx_buffer + rx_tail);
        uint16_t packet_length = (*header >> 16) & 0xFFFF;
        uint16_t packet_status = *header & 0xFFFF;
        
        // Validate packet length (must be reasonable and non-zero)
        if (packet_length < 4 || packet_length > 1518) {
            char debug_msg[80];
            sprintf(debug_msg, "SERIAL: NET - ERROR: Invalid packet length %d, resetting RX buffer\n", packet_length);
            serial_puts(debug_msg);
            // Reset RX buffer to recover from corruption
            outw(rtl8139_device.io_base + RTL8139_RXBUFTAIL, (rx_head - 16) & 0xFFFF);
            rtl8139_device.rx_buffer_pos = rx_head;
            break;
        }
        
        if (packet_status & 0x01) {  // Packet received OK
            uint8_t *packet_data = rtl8139_device.rx_buffer + rx_tail + 4;
            
            // Update statistics
            rtl8139_device.packets_received++;
            rtl8139_device.bytes_received += packet_length;
            
            // Forward packet to IP stack for processing
            extern void ip_receive_packet(uint8_t *packet, uint32_t length);
            ip_receive_packet(packet_data, packet_length - 4);
            
        } else {
            char debug_msg[64];
            sprintf(debug_msg, "SERIAL: NET - ERROR: RX packet error, status: 0x%04X, len: %d\n", 
                    packet_status, packet_length);
            serial_puts(debug_msg);
            rtl8139_device.rx_errors++;
        }
        
        // Advance to next packet (align to 4 bytes)
        rx_tail = (rx_tail + packet_length + 4 + 3) & ~3;
        
        // Wrap around if necessary
        if (rx_tail >= RTL8139_RX_BUF_SIZE) {
            rx_tail -= RTL8139_RX_BUF_SIZE;
        }
    }
    
    // Update RX buffer position
    rtl8139_device.rx_buffer_pos = rx_tail;
    
    // Update the device's RX buffer tail pointer (subtract 16 bytes as required by RTL8139)
    outw(rtl8139_device.io_base + RTL8139_RXBUFTAIL, (rx_tail - 16) & 0xFFFF);
    
    return NULL;
}

void* network_process_tx_complete(void *args) {
    extern void serial_puts(const char *msg);
    
    // Handle TX completion tasks
    // - Update statistics
    // - Free any temporary buffers
    // - Signal waiting applications
    
    return NULL;
}

void rtl8139_print_stats(void) {
    if (!rtl8139_device.initialized) {
        printf("NET: RTL8139 not initialized\n");
        return;
    }
    
    printf("NET: RTL8139 Statistics:\n");
    printf("  Packets sent:     %d\n", rtl8139_device.packets_sent);
    printf("  Packets received: %d\n", rtl8139_device.packets_received);
    printf("  Bytes sent:       %d\n", rtl8139_device.bytes_sent);
    printf("  Bytes received:   %d\n", rtl8139_device.bytes_received);
    printf("  RX errors:        %d\n", rtl8139_device.rx_errors);
    printf("  TX errors:        %d\n", rtl8139_device.tx_errors);
    printf("  MAC address:      %02X:%02X:%02X:%02X:%02X:%02X\n",
           rtl8139_device.mac_addr[0], rtl8139_device.mac_addr[1],
           rtl8139_device.mac_addr[2], rtl8139_device.mac_addr[3],
           rtl8139_device.mac_addr[4], rtl8139_device.mac_addr[5]);
}

// Shell command interface functions
void rtl8139_show_status(void)
{
    if (!rtl8139_device.initialized)
    {
        printf("RTL8139: No network card initialized\n");
        return;
    }
    
    printf("RTL8139 Network Card Status:\n");
    printf("  I/O Base Address: 0x%x\n", rtl8139_device.io_base);
    printf("  IRQ: %d\n", rtl8139_device.irq);
    printf("  MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
        rtl8139_device.mac_addr[0], rtl8139_device.mac_addr[1], rtl8139_device.mac_addr[2],
        rtl8139_device.mac_addr[3], rtl8139_device.mac_addr[4], rtl8139_device.mac_addr[5]);
    
    // Read some status registers
    uint16_t cmd = inw(rtl8139_device.io_base + RTL8139_CMD);
    printf("  Command Register: 0x%04x\n", cmd);
    
    uint16_t isr = inw(rtl8139_device.io_base + RTL8139_ISR);
    printf("  Interrupt Status: 0x%04x\n", isr);
    
    printf("  Initialized: %s\n", rtl8139_device.initialized ? "Yes" : "No");
    printf("  Current TX Buffer: %d\n", rtl8139_device.tx_current);
    printf("  RX Buffer Position: 0x%x\n", rtl8139_device.rx_buffer_pos);
}

void rtl8139_show_stats(void)
{
    rtl8139_print_stats();
}

int rtl8139_send_test_packet(void)
{
    if (!rtl8139_device.initialized)
    {
        printf("RTL8139: Network card not initialized\n");
        return -1;
    }
    
    // Create a simple test packet (ARP request for broadcast)
    uint8_t test_packet[60];
    memset(test_packet, 0, sizeof(test_packet));
    
    // Ethernet header
    memset(test_packet, 0xFF, 6);  // Destination MAC (broadcast)
    memcpy(test_packet + 6, rtl8139_device.mac_addr, 6);  // Source MAC
    test_packet[12] = 0x08;  // EtherType ARP (0x0806) 
    test_packet[13] = 0x06;
    
    // ARP header  
    test_packet[14] = 0x00; test_packet[15] = 0x01;  // Hardware type (Ethernet)
    test_packet[16] = 0x08; test_packet[17] = 0x00;  // Protocol type (IPv4)
    test_packet[18] = 0x06;  // Hardware address length
    test_packet[19] = 0x04;  // Protocol address length  
    test_packet[20] = 0x00; test_packet[21] = 0x01;  // Operation (request)
    
    // Sender hardware address
    memcpy(test_packet + 22, rtl8139_device.mac_addr, 6);
    
    // Sender protocol address (0.0.0.0)
    test_packet[28] = 0x00; test_packet[29] = 0x00; test_packet[30] = 0x00; test_packet[31] = 0x00;
    
    // Target hardware address (unknown, all zeros)
    memset(test_packet + 32, 0x00, 6);
    
    // Target protocol address (192.168.1.1)
    test_packet[38] = 192; test_packet[39] = 168; test_packet[40] = 1; test_packet[41] = 1;
    
    printf("RTL8139: Sending test ARP packet (%d bytes)\n", sizeof(test_packet));
    
    // Send the packet using the existing send function
    rtl8139_send_packet(test_packet, sizeof(test_packet));
    
    printf("RTL8139: Test packet queued for transmission\n");
    
    return 0;
}