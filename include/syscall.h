// syscall.h -- Defines the interface for and structures relating to the syscall dispatch system.
//              Written for JamesM's kernel development tutorials.

#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"

void initialise_syscalls();

#define DECL_SYSCALL0(fn) int syscall_##fn();
#define DECL_SYSCALL1(fn,p1) int syscall_##fn(p1);
#define DECL_SYSCALL2(fn,p1,p2) int syscall_##fn(p1,p2);
#define DECL_SYSCALL3(fn,p1,p2,p3) int syscall_##fn(p1,p2,p3);
#define DECL_SYSCALL4(fn,p1,p2,p3,p4) int syscall_##fn(p1,p2,p3,p4);
#define DECL_SYSCALL5(fn,p1,p2,p3,p4,p5) int syscall_##fn(p1,p2,p3,p4,p5);

#define DEFN_SYSCALL0(fn, num) \
int syscall_##fn() \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num)); \
  return a; \
}

#define DEFN_SYSCALL1(fn, num, P1) \
int syscall_##fn(P1 p1) \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1)); \
  return a; \
}

#define DEFN_SYSCALL2(fn, num, P1, P2) \
int syscall_##fn(P1 p1, P2 p2) \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2)); \
  return a; \
}

#define DEFN_SYSCALL3(fn, num, P1, P2, P3) \
int syscall_##fn(P1 p1, P2 p2, P3 p3) \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d"((int)p3)); \
  return a; \
}

#define DEFN_SYSCALL4(fn, num, P1, P2, P3, P4) \
int syscall_##fn(P1 p1, P2 p2, P3 p3, P4 p4) \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4)); \
  return a; \
}

#define DEFN_SYSCALL5(fn, num) \
int syscall_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) \
{ \
  int a; \
  asm volatile("int $0x80" : "=a" (a) : "0" (num), "b" ((int)p1), "c" ((int)p2), "d" ((int)p3), "S" ((int)p4), "D" ((int)p5)); \
  return a; \
}

DECL_SYSCALL1(os_puts, const char*)
DECL_SYSCALL1(os_getc, int)
DECL_SYSCALL1(os_putc, int)
DECL_SYSCALL0(os_asctime)
DECL_SYSCALL2(os_ls, int, char**)
DECL_SYSCALL2(os_rmdir, int, char**)
DECL_SYSCALL2(os_mkdir, int, char**)
DECL_SYSCALL2(os_cd, int, char**)
DECL_SYSCALL2(os_getcwd, char*, int)
DECL_SYSCALL2(os_chdrive, int, char**)
DECL_SYSCALL2(os_fasm,int, char**)
DECL_SYSCALL0(os_fork)
DECL_SYSCALL2(os_create_elf_process, void*, size_t)
DECL_SYSCALL1(os_exec_process, int)
DECL_SYSCALL1(os_exit_process, int)
DECL_SYSCALL2(os_virtual_alloc, uint32_t, size_t)
DECL_SYSCALL1(os_virtual_free, void*)
DECL_SYSCALL0(os_amp_is_initialized)
DECL_SYSCALL0(os_amp_get_cpu_count)
DECL_SYSCALL0(os_amp_print_status)
int os_fork();
int os_ls(int,char**);
int os_rmdir(int,char**);
int os_mkdir(int,char**);
int os_cd(int,char**);
int os_chdir(const char*);
int os_getcwd(char*,int);
int os_chdrive(int,char**);
int os_fasm(int,char**);
const char *os_asctime();
int os_getc(int);
int os_putc(int);
#endif
