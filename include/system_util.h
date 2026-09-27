#ifndef _SYSTEM_UTIL_H_
#define _SYSTEM_UTIL_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
/* UTIL.C */
int32_t string_split(const char *txt, char delim, char ***tokens);

unsigned char inportb(unsigned short _port);
uint32_t inportl(uint16_t portid);
uint16_t inportw(uint16_t portid);
void outportb(unsigned short _port, unsigned char _data);
void outportw(unsigned short _port, unsigned short _data);
void outportl(uint16_t portid, uint32_t value);
void pause_cpu();
void halt_cpu();
void print_mem_stats();
void print_heap_stats();
#ifdef __cplusplus
}
#endif
#endif