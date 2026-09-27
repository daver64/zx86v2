#include "common.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>

static int read_integer_from_string(char *str)
{
  int res = 0;
  char buffer[128];
  memset(buffer, 0, 128);
  char c = *str;
  str++;
  // putchar(c);
  int i = 0;
  while (isdigit(c) && i < 128 && *str != 0)
  {
    buffer[i++] = c;
    c = *str; // getchar();
    str++;
    // putchar(c);
  }
  res = atoi(buffer);
  return res;
}
static int read_integer()
{
  int res = 0;
  char buffer[128];
  memset(buffer, 0, 128);
  char c = getchar();
  putchar(c);
  int i = 0;
  while (isdigit(c) && i < 128)
  {
    buffer[i++] = c;
    c = getchar();
    putchar(c);
  }
  res = atoi(buffer);
  return res;
}
static double read_float()
{
  int res = 0;
  char buffer[128];
  memset(buffer, 0, 128);
  char c = getchar();
  putchar(c);
  int i = 0;
  if (c == '-')
  {
    buffer[i++] = '-';
  }
  else if (c == '+')
  {
    buffer[i++] = '+';
  }

  while (isdigit(c) && i < 128)
  {
    buffer[i++] = c;
    c = getchar();
    putchar(c);
  }
  if (c == '.' || c == tolower('e'))
  {
    buffer[i++] = c;
    char c = getchar();
    putchar(c);

    while (isdigit(c) && i < 128)
    {
      buffer[i++] = c;
      c = getchar();
      putchar(c);
    }
  }
  double val = atof(buffer);
  // printf("atof returned %lf from %s\n",val,buffer);
  return val;
}
static double read_float_from_string(char *str)
{
  int res = 0;
  char buffer[128];
  memset(buffer, 0, 128);
  char c = *str; // getchar();
  str++;
  // putchar(c);
  int i = 0;
  if (c == '-')
  {
    buffer[i++] = '-';
  }
  else if (c == '+')
  {
    buffer[i++] = '+';
  }

  while (isdigit(c) && i < 128 && *str!=0)
  {
    buffer[i++] = c;
    c = *str;
    str++;
    // c = getchar();
    // putchar(c);
  }
  if (c == '.' || c == tolower('e'))
  {
    buffer[i++] = c;
    c = *str;
    str++;
    // char c = getchar();
    // putchar(c);

    while (isdigit(c) && i < 128 && *str!=0)
    {
      buffer[i++] = c;
      c = *str;
      str++;
      // c = getchar();
      // putchar(c);
    }
  }
  double val = atof(buffer);
  // printf("atof returned %lf from %s\n",val,buffer);
  return val;
}
static int s_isspace(char c)
{
  return (isspace(c) || c == '\n');
}
static char *read_into_string(char *str)
{
  char *ptr = str;
  *ptr = getchar();
  putchar(*ptr);
  while (!s_isspace(*ptr++))
  {
    *ptr = getchar();
    putchar(*ptr);
  }
  *(--ptr) = 0;
  return str;
}
static char *read_into_string_from_string(char *str, char *src_str)
{
  char *ptr = str;
  *ptr = *src_str;//getchar();
  src_str++;
  //putchar(*ptr);
  while (!s_isspace(*ptr++) && src_str!=0)
  {
    *ptr = *src_str;//getchar();
    src_str++;
    //putchar(*ptr);
  }
  *(--ptr) = 0;
  return str;
}
/*
  Very rudimentary scanf, just enough to grab some simple things...
*/
int scanf(char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  char *str = format;
  int ret = 0;
  while (*str != 0)
  {
    if ((*str != '%') || (*(str + 1) == 0))
    {
      //
    }
    else
    {
      switch (*(str + 1))
      {

      // characters
      case 'c':
      {
        void *dest_addr = va_arg(ap, void *);
        char *cptr = ((char *)dest_addr);
        *cptr = getchar();
        putchar(*cptr);
        ret++;
      }
      // integers
      case 'd':
      {
        void *dest_addr = va_arg(ap, void *);
        int *iptr = ((int *)dest_addr);
        *iptr = read_integer();
        ret++;
      }
      break;

      // unsigned integers
      case 'z':
      case 'u':
      {
        void *dest_addr = va_arg(ap, void *);
        unsigned int *uiptr = ((unsigned int *)dest_addr);
        *uiptr = read_integer();
        ret++;
      }
      break;

      // 32 and 64 bit floating point.
      case 'f':
      case 'g':
      {
        void *dest_addr = va_arg(ap, void *);
        if (*(str + 1) == 'f')
        {
          float *fptr = ((float *)dest_addr);
          *fptr = (float)read_float();
        }
        else if (*(str + 1) == 'g')
        {
          double *fptr = ((double *)dest_addr);
          *fptr = read_float();
        }
        ret++;
      }
      break;

      // strings
      case 's':
      {
        void *dest_addr = va_arg(ap, void *);
        char *cptr = ((char *)dest_addr);
        cptr = read_into_string(cptr);
      }
      break;
      }

      str += 2;
    }
  }
  va_end(ap);
  return ret;
}

int sscanf(char *buff, char *fmt, ...)
{
  printf("not implemented\n");
  return 0;
}

int fsscanf(FILE *fp, char *fmt, ...)
{
  printf("not implemented\n");
}
