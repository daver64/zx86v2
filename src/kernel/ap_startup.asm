; ap_startup.asm - Application Processor startup code for SMP
; This code gets copied to 0x8000 and executed by secondary CPUs
; Must be position-independent and fit in one page (4KB)

[BITS 16]
; Note: No ORG directive - this is ELF format, will be copied to 0x8000 at runtime

section .text

; This is the entry point that gets jumped to when a secondary CPU receives STARTUP IPI
global ap_startup_begin
global ap_startup_end

ap_startup_begin:
    cli                     ; Disable interrupts
    cld                     ; Clear direction flag
    
    ; Write a debug byte to indicate we reached the AP startup code
    mov al, 0xAA
    mov [0x8FFC], al        ; Write debug marker
    
    ; Set up basic segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    
    ; Write another debug byte
    mov al, 0xBB
    mov [0x8FFD], al        ; Write debug marker
    
    ; Load our temporary GDT
    lgdt [temp_gdt_desc - ap_startup_begin + 0x8000]
    
    ; Write debug byte after GDT load
    mov al, 0xCC
    mov [0x8FFE], al        ; Write debug marker
    
    ; Enter protected mode
    mov eax, cr0
    or eax, 1               ; Set PE bit
    mov cr0, eax
    
    ; Far jump to 32-bit code segment
    jmp 0x08:(protected_mode_entry - ap_startup_begin + 0x8000)

[BITS 32]
protected_mode_entry:
    ; Write debug byte to indicate we reached 32-bit mode
    mov al, 0xDD
    mov [0x8FFF], al        ; Write debug marker
    
    ; Set up 32-bit segments
    mov ax, 0x10            ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up stack - use the allocated stack for this CPU
    ; The stack pointer should be passed from the kernel
    mov esp, [cpu_stack_ptr - ap_startup_begin + 0x8000]  ; Load allocated stack pointer
    
    ; Enable paging by loading the kernel page directory
    ; The kernel page directory physical address should be stored at a known location
    mov eax, [kernel_page_dir_phys - ap_startup_begin + 0x8000]  ; Load from known memory location
    mov cr3, eax                     ; Load page directory
    
    mov eax, cr0
    or eax, 0x80000000              ; Set PG bit
    mov cr0, eax                    ; Enable paging
    
    ; Jump to virtual address space and call C function
    ; The C function address should be stored at a known location
    mov eax, [secondary_cpu_main_addr - ap_startup_begin + 0x8000]
    mov ebx, [cpu_id_value - ap_startup_begin + 0x8000]  ; Load CPU ID
    push ebx                        ; Push CPU ID parameter
    call eax                        ; Call secondary_cpu_main(cpu_id)
    
    ; If we return, halt
ap_halt:
    cli
    hlt
    jmp ap_halt

; Temporary GDT for switching to protected mode
align 8
temp_gdt:
    dq 0x0000000000000000           ; Null descriptor
    dq 0x00CF9A000000FFFF           ; Code segment (0x08)
    dq 0x00CF92000000FFFF           ; Data segment (0x10)

temp_gdt_desc:
    dw temp_gdt_desc - temp_gdt - 1 ; Limit
    dd temp_gdt - ap_startup_begin + 0x8000  ; Base (adjusted for runtime address)

; Data storage locations - these will be filled by kernel before starting AP
align 4
kernel_page_dir_phys:
    dd 0x00000000                   ; Physical address of kernel page directory

secondary_cpu_main_addr:
    dd 0x00000000                   ; Virtual address of secondary_cpu_main function

cpu_stack_ptr:
    dd 0x00000000                   ; Stack pointer for this CPU

cpu_id_value:
    dd 0x00000000                   ; CPU ID for this CPU

ap_startup_end: