/*

Just playing around with ideas.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <basic.h>

typedef struct bc_keyword
{
    const char *keyword;
    int (*function_t)(int, char **);
} bc_keyword_t;

#define BC_MAX_DIGITS (255)
#define BC_MAX_LABEL (255)
static int quit = false;
static int echo = true;
int look = ' ';

bc_keyword_t keywords[] = {
    {"for", NULL},
    {NULL, NULL}};

int bc_iskeyword(const char *str)
{
    for (int i = 0; i <= (sizeof(keywords) / sizeof(bc_keyword_t)); i++)
    {
        bc_keyword_t *kw = &keywords[i];
        if (!strcmp(str, kw->keyword))
        {
            return true;
        }
    }
    return false;
}
int bc_getchar()
{
    look = getchar();
    if(echo)
    {
        putchar(look);
    }
}
int bc_isws()
{
    return (look == ' ' || look == '\t' || look == '\n');
}
int bc_skipws()
{
    while (bc_isws())
        bc_getchar();
}

int bc_isdigit(char c)
{
    return (c >= '0' && c <= '9');
}

int bc_isalpha(char c)
{
    return (toupper(c) >= 'A' && toupper(c) <= 'Z');
}

int bc_isaddop(char c)
{
    return (c == '+' || c == '-');
}

int bc_ismulop(char c)
{
    return (c == '*' || c == '/');
}
int bc_expect(char c)
{
    bc_skipws();
    return (look==c);
}
char *bc_readdigits()
{
    char *digitsbuffer = (char *)malloc(BC_MAX_DIGITS);
    memset(digitsbuffer, 0, BC_MAX_DIGITS);
    size_t count = 0;
    while (bc_isdigit(look) && count < BC_MAX_DIGITS)
    {
        digitsbuffer[count] = look;
        count++;
        bc_getchar();
    }
    return digitsbuffer;
}

char *bc_readlabel()
{
    char *labelbuffer = malloc(BC_MAX_LABEL);
    memset(labelbuffer, 0, BC_MAX_LABEL);
    size_t count = 0;
    while (bc_isalpha(look) && !bc_isws(look) && count < BC_MAX_LABEL)
    {
        labelbuffer[count] = look;
        count++;
        bc_getchar();
    }
    return labelbuffer;
}
int bc_getnumber()
{
    char *digits = bc_readdigits();
    int32_t number = strtol(digits, NULL, 10);
    free(digits);
    return number;
}
int bc_printint(int number, int base)
{
    if(base==10)
    {
        printf("%d",number);
    }
    else if(base==16)
    {
        printf("%X",number);
    }
    else
    {
        return 1;
    }
    return 0;
}
int bc_printfloat(float f)
{
    printf("%f",f);
}
int bc_printstring(char *str)
{
    printf("%s",str);
}
uint32_t *basic_memory=0;
uint32_t basic_memory_size=0x100000;

int basic_main(int argc, char *argv[])
{
    basic_memory=malloc(basic_memory_size);
    printf("BASIC Ver 0.1\n");
    printf("%u Bytes Basic Memory @ 0x%08X\n",basic_memory_size,basic_memory);
    do
    {
        bc_skipws();
        if(bc_isdigit(look))
        {
            int num=bc_getnumber();
            printf("number=%d\n",num);
        }
        else if(bc_isalpha(look))
        {
            char *label=bc_readlabel();
            printf("label='%s'\n",label);
            if(!strcmp(label,"quit"))
            {
                quit=true;
            }
            free(label);
        }
        else
        {
            printf("syntax error\n");
            quit=true;
        }
    } while (!quit);
    
    free(basic_memory);
    return 0;
}