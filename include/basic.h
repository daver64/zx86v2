#ifndef _ZX86_BASIC_H_
#define _ZX86_BASIC_H_

int bc_iskeyword(const char *str);
int bc_getchar();
int bc_isws();
int bc_skipws();
int bc_isdigit(char c);
int bc_isalpha(char c);
int bc_isaddop(char c);
int bc_ismulop(char c);
int bc_expect(char c);
char *bc_readdigits();
char *bc_readlabel();
int bc_getnumber();
int bc_printint(int number, int base);
int bc_printfloat(float f);
int bc_printstring(char *str);
int basic_main(int argc, char *argv[]);
#endif

