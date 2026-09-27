/*
 * errno.h - Error number definitions (BSD-style)
 * ZX86v2 Operating System
 */

#ifndef _ERRNO_H_
#define _ERRNO_H_

#include <stdarg.h>
#include <stddef.h>

/* Include the comprehensive BSD-style error definitions */
#include <sys/errno.h>

/* Global errno variable */
extern int errno;

/* Function declarations */
#ifdef __cplusplus
extern "C" {
#endif

char *strerror(int errnum);
int strerror_r(int errnum, char *buf, size_t buflen);
void perror(const char *s);

/* BSD-style error reporting functions */
void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);
void vwarn(const char *fmt, va_list ap);
void vwarnx(const char *fmt, va_list ap);

void err(int eval, const char *fmt, ...);
void errx(int eval, const char *fmt, ...);
void verr(int eval, const char *fmt, va_list ap);
void verrx(int eval, const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif /* _ERRNO_H_ */
