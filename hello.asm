format elf executable
entry start

segment readable executable

start:
    mov eax,0
    mov ebx,msg
    int 0x80

    mov eax,42
    ret

segment readable writeable

msg db 'Hello, zx86!',0xA,0