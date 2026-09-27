// pci.h -- PCI device enumeration and management
// For ZX86v2 kernel

#ifndef PCI_H
#define PCI_H

#include "common.h"

// PCI configuration space registers
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI configuration space offsets
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_CLASS_CODE      0x08
#define PCI_HEADER_TYPE     0x0E
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24

// PCI command register bits
#define PCI_COMMAND_IO          0x01
#define PCI_COMMAND_MEMORY      0x02
#define PCI_COMMAND_MASTER      0x04

// PCI class codes
#define PCI_CLASS_VGA           0x030000

// QEMU Standard VGA device IDs
#define QEMU_VGA_VENDOR_ID      0x1234
#define QEMU_VGA_DEVICE_ID      0x1111

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    uint32_t bar[6];
} pci_device_t;

// PCI functions
void initialise_pci();
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_class(uint32_t class_code);
uint32_t pci_get_bar(pci_device_t *device, int bar_num);
void pci_enable_memory(pci_device_t *device);

#endif