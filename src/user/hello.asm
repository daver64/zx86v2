format elf executable
entry start

segment readable executable

start:
    mov eax,42
    ret

segment readable writeable

msg db 'hello',0