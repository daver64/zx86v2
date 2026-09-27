#ifndef _STDC_STDDEF_H_
#define _STDC_STDDEF_H_

#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif

typedef __SIZE_TYPE__ size_t;
typedef long int ssize_t;

typedef __PTRDIFF_TYPE__ ptrdiff_t;

//#ifndef __WCHAR_TYPE__
#if !defined(__cplusplus)
typedef __WCHAR_TYPE__ wchar_t;
#endif
//#endif

#define offsetof(st, m) \
    ((size_t)((char *)&((st *)(0))->m - (char *)0))

#endif
