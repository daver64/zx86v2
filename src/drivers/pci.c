// pci.c -- PCI device enumeration and management
// For ZX86v2 kernel

#include "pci.h"
#include "common.h"

static pci_device_t pci_devices[256];
static int pci_device_count = 0;

uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1 << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint32_t pci_get_bar(pci_device_t *device, int bar_num) {
    if (!device || bar_num < 0 || bar_num >= 6) {
        return 0;
    }
    
    uint32_t offset = 0x10 + (bar_num * 4); // BAR0 starts at offset 0x10
    return pci_config_read_dword(device->bus, device->device, device->function, offset);
}

pci_device_t *pci_find_class(uint32_t class_code) {
    for (int i = 0; i < pci_device_count; i++) {
        if ((pci_devices[i].class_code & 0xFFFF00) == class_code) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

void pci_enable_memory(pci_device_t *device) {
    if (!device) return;
    
    uint16_t command = pci_config_read_word(device->bus, device->device, device->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_write_config(device->bus, device->device, device->function, PCI_COMMAND, command);
}

static void pci_scan_device(uint8_t bus, uint8_t device) {
    uint8_t function = 0;
    
    uint16_t vendor_id = pci_config_read_word(bus, device, function, PCI_VENDOR_ID);
    if (vendor_id == 0xFFFF) return; // Device doesn't exist
    
    pci_device_t *dev = &pci_devices[pci_device_count];
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_config_read_word(bus, device, function, PCI_DEVICE_ID);
    dev->class_code = pci_config_read_dword(bus, device, function, PCI_CLASS_CODE) >> 8;
    
    // Read all BARs
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_config_read_dword(bus, device, function, PCI_BAR0 + (i * 4));
    }
    
    printf("PCI device %02x:%02x.%x - %04x:%04x (class %06x)", 
           bus, device, function, dev->vendor_id, dev->device_id, dev->class_code);
    
    // Special logging for VGA devices
    if ((dev->class_code & 0xFFFF00) == PCI_CLASS_VGA) {
        printf(" [VGA DEVICE FOUND!]");
    }
    printf("\n");
    
    pci_device_count++;
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        pci_scan_device(bus, device);
        if (pci_device_count >= 256) return; // Prevent overflow
    }
}

void initialise_pci() {
    printf("Initializing PCI subsystem...\n");
    pci_device_count = 0;
    
    // Scan bus 0 (primary bus)
    pci_scan_bus(0);
    
    printf("PCI initialization complete. Found %d devices.\n", pci_device_count);
    
    // Look specifically for VGA devices
    printf("Searching for VGA devices...\n");
    pci_device_t *vga = pci_find_class(PCI_CLASS_VGA);
    if (vga) {
        printf("Found VGA device: %04x:%04x at %02x:%02x.%x\n", 
               vga->vendor_id, vga->device_id, vga->bus, vga->device, vga->function);
    } else {
        printf("No VGA devices found in PCI scan\n");
    }
}