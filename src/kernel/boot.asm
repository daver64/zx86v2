MBOOT_PAGE_ALIGN    equ 1<<0    ; Load kernel and modules on a page boundary
MBOOT_MEM_INFO      equ 1<<1    ; Provide your kernel with memory info
MBOOT_HEADER_MAGIC  equ 0x1BADB002 ; Multiboot Magic value
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)


[BITS 32]

[GLOBAL mboot]
[EXTERN code]
[EXTERN bss]
[EXTERN end]

section .mbheader

mboot:
    dd  MBOOT_HEADER_MAGIC      ; magic
    dd  MBOOT_HEADER_FLAGS      ; flags
    dd  MBOOT_CHECKSUM          ; checksum
    dd  mboot                   ; header_addr
    dd  code                    ; load_addr
    dd  bss                     ; load_end_addr
    dd  end                     ; bss_end_addr
    dd  start                   ; entry_addr

section .text

[GLOBAL start]
[EXTERN kmain]

start:
    ; Load multiboot information:
	mov esi, 0x400000
	push ebx 
    cli
    call kmain
    jmp $

align 16

global inportsw
inportsw:
	push edi
	push edx
	push ecx

	mov edx,[esp+16]
	mov edi,[esp+20]
	mov ecx,[esp+24]

	xor eax,eax
.insw_startloop:
	cmp eax,ecx
	je .insw_end

	insw

	inc eax
	jmp .insw_startloop

.insw_end:
	pop ecx
	pop edx
	pop edi
ret


align 16
global outportsw
outportsw:
	push esi
	push edx
	push ecx
	mov edx,[esp+16]
	mov esi,[esp+20]
	mov ecx,[esp+24]
	
	xor eax,eax
.outsw_startloop:
	cmp eax,ecx
	je .outsw_end

	outsw

	inc eax
	jmp .outsw_startloop
.outsw_end:
	pop ecx
	pop edx
	pop esi
ret