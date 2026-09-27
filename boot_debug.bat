@echo off
echo Running ZX86v2 with debug output and graphics support
echo Available VGA options:
echo   -vga std      = QEMU Standard VGA (device 1234:1111) - what we want
echo   -vga cirrus   = Cirrus Logic VGA
echo   -vga vmware   = VMware SVGA
echo   -vga qxl      = QXL paravirtual graphics
echo   -vga virtio   = VirtIO GPU
echo   -vga none     = No VGA card
echo.
echo Audio configuration:
echo   -device sb16  = Sound Blaster 16 (I/O port 0x220, IRQ 5, DMA 1/5)
echo   -audiodev dsound = DirectSound audio output for Windows
echo   -machine pcspk-audiodev = Connect PC speaker to same audio device
echo   Using basic audio settings for compatibility
echo.
echo Network configuration:
echo   -netdev user,id=net0,hostfwd=tcp::2007-:7 = User mode networking (NAT) with port forwarding
echo   -device rtl8139,netdev=net0 = RTL8139 network card
echo   Port forwarding: Windows localhost:2007 -> Guest OS port 7 (TCP echo server)
echo   This provides a virtual RTL8139 NIC for network driver testing
echo.
echo Using QEMU Standard VGA (-vga std) and Sound Blaster 16 with audio output
echo Serial output will appear in this console
echo Press Ctrl+C to stop QEMU
echo.
qemu-system-x86_64 -smp 8 -m 1024 -kernel kernel.bin -hda dosdisc.img -vga std -audiodev dsound,id=snd0 -device sb16,audiodev=snd0,iobase=0x220,irq=5,dma=1,dma16=5 -machine pcspk-audiodev=snd0 -netdev user,id=net0,hostfwd=tcp::2007-:7 -device rtl8139,netdev=net0 -serial stdio -monitor none 2>nul
reset