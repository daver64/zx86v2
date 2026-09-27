ifeq ($(OS),Windows_NT)
CC=i686-elf-gcc
CPP=i686-elf-g++
LD=i686-elf-ld
OBJCOPY=i686-elf-objcopy
else
CC=i686-elf-gcc
CPP=i686-elf-g++
LD=i686-elf-ld
OBJCOPY=i686-elf-objcopy
endif


ASM=nasm
OBJDIR=obj
SRCDIR=src
KOBJS:=$(OBJDIR)/boot.o $(OBJDIR)/gdt.o $(OBJDIR)/interrupt.o $(OBJDIR)/process.o \
 $(OBJDIR)/common.o $(OBJDIR)/printf.o $(OBJDIR)/scanf.o $(OBJDIR)/descriptor_tables.o $(OBJDIR)/shell.o \
 $(OBJDIR)/isr.o $(OBJDIR)/kheap.o $(OBJDIR)/main.o $(OBJDIR)/ordered_array.o \
 $(OBJDIR)/paging.o $(OBJDIR)/syscall.o $(OBJDIR)/task.o $(OBJDIR)/timer.o $(OBJDIR)/kb.o $(OBJDIR)/bget.o \
 $(OBJDIR)/string.o $(OBJDIR)/stdio.o $(OBJDIR)/malloc.o $(OBJDIR)/errno.o $(OBJDIR)/unistd.o $(OBJDIR)/rtc.o $(OBJDIR)/serial.o $(OBJDIR)/ctype.o \
 $(OBJDIR)/math.o $(OBJDIR)/sin.o $(OBJDIR)/cos.o $(OBJDIR)/disc.o \
 $(OBJDIR)/dfs.o $(OBJDIR)/diskio.o $(OBJDIR)/ff.o $(OBJDIR)/ffsystem.o $(OBJDIR)/ffunicode.o \
 $(OBJDIR)/scheme.o  $(OBJDIR)/scheme-api.o $(OBJDIR)/environment.o $(OBJDIR)/util.o \
 $(OBJDIR)/basic.o $(OBJDIR)/elf-load.o $(OBJDIR)/elf-module-i386.o $(OBJDIR)/elf-module.o  $(OBJDIR)/sha-256.o \
 $(OBJDIR)/lzw.o $(OBJDIR)/db.o $(OBJDIR)/fasm.o $(OBJDIR)/fasm_io.o $(OBJDIR)/vi_simple.o $(OBJDIR)/disasm.o \
 $(OBJDIR)/pci.o $(OBJDIR)/qemu_vga.o $(OBJDIR)/framebuffer.o $(OBJDIR)/font.o $(OBJDIR)/terminal.o $(OBJDIR)/bitmap.o $(OBJDIR)/sound.o \
 $(OBJDIR)/rtl8139.o $(OBJDIR)/ip.o $(OBJDIR)/icmp.o $(OBJDIR)/tcp.o \
 $(OBJDIR)/amp.o $(OBJDIR)/amp_cpu.o $(OBJDIR)/ap_startup.o \
 $(OBJDIR)/blockdev.o $(OBJDIR)/vfs.o $(OBJDIR)/fat_vfs.o $(OBJDIR)/devfs.o $(OBJDIR)/ramdisk.o $(OBJDIR)/fat32_format.o 

CPPFLAGS=-m32 -std=c++0x -ffreestanding  -fno-exceptions -fno-rtti -fno-stack-protector -I./include
CFLAGS=-m32  -ffreestanding  -fno-stack-protector -I./include

#
# Linker settings are environment and compiler specific. There's got to be an easier way to link
# libgcc but I haven't found anything yet other than suggestions on forums to use gcc to do the linking, which looks
# awkward. Adjust as needed to your local GCC linker settings.
#
# You can get the libgcc settings with 
# gcc -print-libgcc-file-name , invoke gcc with the local copy eg: i686-elf-gcc
#
ifeq ($(OS),Windows_NT)
LDFLAGS=-Map kernel.map -T link.ld -static -LC:/Users/daver/LocalApps/elf32/lib/gcc/i686-elf/13.2.0 -lgcc
else
LDFLAGS= -Map kernel.map -static -L/usr/local/cross/lib/gcc/i686-elf/13.2.0/ -lgcc -T link.ld
endif
#
#
#

ASMFLAGS=-felf
KERNELBINARY:= kernel.bin

all: $(KERNELBINARY) 

$(KERNELBINARY) :$(KOBJS)
	$(LD) -o $(KERNELBINARY) $(KOBJS)  $(LDFLAGS) 

$(OBJDIR)/boot.o : $(SRCDIR)/kernel/boot.asm
	$(ASM) $(ASMFLAGS) -o $(OBJDIR)/boot.o $(SRCDIR)/kernel/boot.asm

$(OBJDIR)/gdt.o : $(SRCDIR)/kernel/gdt.asm
	$(ASM) $(ASMFLAGS) -o $(OBJDIR)/gdt.o $(SRCDIR)/kernel/gdt.asm

$(OBJDIR)/interrupt.o : $(SRCDIR)/kernel/interrupt.asm
	$(ASM) $(ASMFLAGS) -o $(OBJDIR)/interrupt.o $(SRCDIR)/kernel/interrupt.asm

$(OBJDIR)/process.o : $(SRCDIR)/kernel/process.asm
	$(ASM) $(ASMFLAGS) -o $(OBJDIR)/process.o $(SRCDIR)/kernel/process.asm

$(OBJDIR)/common.o : $(SRCDIR)/kernel/common.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/common.c -o $(OBJDIR)/common.o

$(OBJDIR)/printf.o : $(SRCDIR)/libc/printf.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/printf.c -o $(OBJDIR)/printf.o

$(OBJDIR)/scanf.o : $(SRCDIR)/libc/scanf.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/scanf.c -o $(OBJDIR)/scanf.o

$(OBJDIR)/descriptor_tables.o : $(SRCDIR)/kernel/descriptor_tables.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/descriptor_tables.c -o $(OBJDIR)/descriptor_tables.o

$(OBJDIR)/shell.o : $(SRCDIR)/user/shell.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/shell.c -o $(OBJDIR)/shell.o

$(OBJDIR)/vi_simple.o : $(SRCDIR)/user/vi_simple.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/vi_simple.c -o $(OBJDIR)/vi_simple.o

$(OBJDIR)/disasm.o : $(SRCDIR)/user/disasm.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/disasm.c -o $(OBJDIR)/disasm.o

$(OBJDIR)/isr.o : $(SRCDIR)/kernel/isr.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/isr.c -o $(OBJDIR)/isr.o

$(OBJDIR)/kheap.o : $(SRCDIR)/kernel/kheap.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/kheap.c -o $(OBJDIR)/kheap.o

$(OBJDIR)/main.o : $(SRCDIR)/kernel/main.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/main.c -o $(OBJDIR)/main.o

$(OBJDIR)/ordered_array.o : $(SRCDIR)/kernel/ordered_array.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/ordered_array.c -o $(OBJDIR)/ordered_array.o

$(OBJDIR)/paging.o : $(SRCDIR)/kernel/paging.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/paging.c -o $(OBJDIR)/paging.o

$(OBJDIR)/syscall.o : $(SRCDIR)/kernel/syscall.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/syscall.c -o $(OBJDIR)/syscall.o

$(OBJDIR)/task.o : $(SRCDIR)/kernel/task.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/task.c -o $(OBJDIR)/task.o

$(OBJDIR)/timer.o : $(SRCDIR)/kernel/timer.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/timer.c -o $(OBJDIR)/timer.o

$(OBJDIR)/kb.o : $(SRCDIR)/drivers/kb.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/kb.c -o $(OBJDIR)/kb.o

$(OBJDIR)/bget.o : $(SRCDIR)/user/bget.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/bget.c -o $(OBJDIR)/bget.o

$(OBJDIR)/string.o : $(SRCDIR)/libc/string.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/string.c -o $(OBJDIR)/string.o

$(OBJDIR)/stdio.o : $(SRCDIR)/libc/stdio.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/stdio.c -o $(OBJDIR)/stdio.o

$(OBJDIR)/malloc.o : $(SRCDIR)/libc/malloc.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/malloc.c -o $(OBJDIR)/malloc.o

$(OBJDIR)/errno.o : $(SRCDIR)/libc/errno.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/errno.c -o $(OBJDIR)/errno.o

$(OBJDIR)/unistd.o : $(SRCDIR)/libc/unistd.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/unistd.c -o $(OBJDIR)/unistd.o

$(OBJDIR)/rtc.o : $(SRCDIR)/drivers/rtc.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/rtc.c -o $(OBJDIR)/rtc.o

$(OBJDIR)/serial.o : $(SRCDIR)/drivers/serial.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/serial.c -o $(OBJDIR)/serial.o

$(OBJDIR)/rtl8139.o : $(SRCDIR)/drivers/rtl8139.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/rtl8139.c -o $(OBJDIR)/rtl8139.o

$(OBJDIR)/ip.o : $(SRCDIR)/network/ip.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/network/ip.c -o $(OBJDIR)/ip.o

$(OBJDIR)/icmp.o : $(SRCDIR)/network/icmp.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/network/icmp.c -o $(OBJDIR)/icmp.o

$(OBJDIR)/tcp.o : $(SRCDIR)/network/tcp.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/network/tcp.c -o $(OBJDIR)/tcp.o

$(OBJDIR)/ctype.o : $(SRCDIR)/libc/ctype.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/ctype.c -o $(OBJDIR)/ctype.o

$(OBJDIR)/math.o : $(SRCDIR)/libc/math.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/math.c -o $(OBJDIR)/math.o

$(OBJDIR)/sin.o : $(SRCDIR)/libc/sin.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/sin.c -o $(OBJDIR)/sin.o

$(OBJDIR)/cos.o : $(SRCDIR)/libc/cos.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/cos.c -o $(OBJDIR)/cos.o

$(OBJDIR)/disc.o : $(SRCDIR)/drivers/disc.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/disc.c -o $(OBJDIR)/disc.o

$(OBJDIR)/lzw.o : $(SRCDIR)/user/lzw.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/lzw.c -o $(OBJDIR)/lzw.o

$(OBJDIR)/dfs.o : $(SRCDIR)/drivers/dfs.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/dfs.c -o $(OBJDIR)/dfs.o

$(OBJDIR)/diskio.o : $(SRCDIR)/drivers/diskio.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/diskio.c -o $(OBJDIR)/diskio.o

$(OBJDIR)/ff.o : $(SRCDIR)/drivers/ff.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/ff.c -o $(OBJDIR)/ff.o

$(OBJDIR)/ffsystem.o : $(SRCDIR)/drivers/ffsystem.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/ffsystem.c -o $(OBJDIR)/ffsystem.o

$(OBJDIR)/ffunicode.o : $(SRCDIR)/drivers/ffunicode.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/ffunicode.c -o $(OBJDIR)/ffunicode.o

$(OBJDIR)/scheme.o : $(SRCDIR)/user/scheme.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/scheme.c -o $(OBJDIR)/scheme.o

$(OBJDIR)/scheme-api.o : $(SRCDIR)/user/scheme-api.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/scheme-api.c -o $(OBJDIR)/scheme-api.o

$(OBJDIR)/environment.o : $(SRCDIR)/user/environment.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/environment.c -o $(OBJDIR)/environment.o

$(OBJDIR)/util.o : $(SRCDIR)/kernel/util.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/util.c -o $(OBJDIR)/util.o

$(OBJDIR)/basic.o : $(SRCDIR)/user/basic.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/basic.c -o $(OBJDIR)/basic.o

$(OBJDIR)/elf-load.o : $(SRCDIR)/drivers/elf-load.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/elf-load.c -o $(OBJDIR)/elf-load.o

$(OBJDIR)/elf-module-i386.o : $(SRCDIR)/drivers/elf-module-i386.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/elf-module-i386.c -o $(OBJDIR)/elf-module-i386.o

$(OBJDIR)/elf-module.o : $(SRCDIR)/drivers/elf-module.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/elf-module.c -o $(OBJDIR)/elf-module.o

$(OBJDIR)/sha-256.o : $(SRCDIR)/libc/sha-256.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/libc/sha-256.c -o $(OBJDIR)/sha-256.o

$(OBJDIR)/db.o : $(SRCDIR)/user/db.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/user/db.c -o $(OBJDIR)/db.o


$(OBJDIR)/fasm.o : fasm/source/zx86/fasm.asm
	fasm fasm/source/zx86/fasm.asm $(OBJDIR)/fasm.o

$(OBJDIR)/fasm_io.o : $(SRCDIR)/kernel/fasm_io.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/fasm_io.c -o $(OBJDIR)/fasm_io.o

$(OBJDIR)/pci.o : $(SRCDIR)/drivers/pci.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/pci.c -o $(OBJDIR)/pci.o

$(OBJDIR)/qemu_vga.o : $(SRCDIR)/drivers/qemu_vga.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/qemu_vga.c -o $(OBJDIR)/qemu_vga.o

$(OBJDIR)/framebuffer.o : $(SRCDIR)/graphics/framebuffer.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/graphics/framebuffer.c -o $(OBJDIR)/framebuffer.o

$(OBJDIR)/font.o : $(SRCDIR)/graphics/font.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/graphics/font.c -o $(OBJDIR)/font.o

$(OBJDIR)/terminal.o : $(SRCDIR)/graphics/terminal.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/graphics/terminal.c -o $(OBJDIR)/terminal.o

$(OBJDIR)/bitmap.o : $(SRCDIR)/graphics/bitmap.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/graphics/bitmap.c -o $(OBJDIR)/bitmap.o

$(OBJDIR)/sound.o : $(SRCDIR)/drivers/sound.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/sound.c -o $(OBJDIR)/sound.o

$(OBJDIR)/amp.o : $(SRCDIR)/kernel/amp.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/amp.c -o $(OBJDIR)/amp.o

$(OBJDIR)/amp_cpu.o : $(SRCDIR)/kernel/amp_cpu.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/amp_cpu.c -o $(OBJDIR)/amp_cpu.o

$(OBJDIR)/amp_examples.o : $(SRCDIR)/kernel/amp_examples.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/amp_examples.c -o $(OBJDIR)/amp_examples.o

$(OBJDIR)/ap_startup.o : $(SRCDIR)/kernel/ap_startup.asm
	$(ASM) $(ASMFLAGS) -o $(OBJDIR)/ap_startup.o $(SRCDIR)/kernel/ap_startup.asm

$(OBJDIR)/blockdev.o : $(SRCDIR)/drivers/blockdev.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/drivers/blockdev.c -o $(OBJDIR)/blockdev.o

$(OBJDIR)/vfs.o : $(SRCDIR)/fs/vfs.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/fs/vfs.c -o $(OBJDIR)/vfs.o

$(OBJDIR)/fat_vfs.o : $(SRCDIR)/fs/fat_vfs.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/fs/fat_vfs.c -o $(OBJDIR)/fat_vfs.o

$(OBJDIR)/devfs.o : $(SRCDIR)/fs/devfs.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/fs/devfs.c -o $(OBJDIR)/devfs.o

$(OBJDIR)/ramdisk.o : $(SRCDIR)/kernel/ramdisk.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/ramdisk.c -o $(OBJDIR)/ramdisk.o

$(OBJDIR)/fat32_format.o : $(SRCDIR)/kernel/fat32_format.c
	$(CC) $(CFLAGS) -c $(SRCDIR)/kernel/fat32_format.c -o $(OBJDIR)/fat32_format.o

.PHONY: clean
clean:
	rm -f $(OBJDIR)/*.o kernel.bin

