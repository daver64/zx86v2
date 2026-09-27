/*
 * errno.c - Enhanced error handling system (BSD-style)
 * ZX86v2 Operating System
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Global errno variable
int errno = 0;

/*
 * Error message strings for errno values we actually support
 */
static const char *error_messages[] = {
	[0] = "Success",
	[EPERM] = "Operation not permitted",
	[ENOENT] = "No such file or directory",
	[ESRCH] = "No such process",
	[EINTR] = "Interrupted system call",
	[EIO] = "Input/output error",
	[ENXIO] = "No such device or address",
	[E2BIG] = "Argument list too long",
	[ENOEXEC] = "Exec format error",
	[EBADF] = "Bad file descriptor",
	[ECHILD] = "No child processes",
	[EDEADLK] = "Resource deadlock avoided",
	[ENOMEM] = "Cannot allocate memory",
	[EACCES] = "Permission denied",
	[EFAULT] = "Bad address",
	[ENOTBLK] = "Block device required",
	[EBUSY] = "Device or resource busy",
	[EEXIST] = "File exists",
	[EXDEV] = "Invalid cross-device link",
	[ENODEV] = "No such device",
	[ENOTDIR] = "Not a directory",
	[EISDIR] = "Is a directory",
	[EINVAL] = "Invalid argument",
	[ENFILE] = "Too many open files in system",
	[EMFILE] = "Too many open files",
	[ENOTTY] = "Inappropriate ioctl for device",
	[ETXTBSY] = "Text file busy",
	[EFBIG] = "File too large",
	[ENOSPC] = "No space left on device",
	[ESPIPE] = "Illegal seek",
	[EROFS] = "Read-only file system",
	[EMLINK] = "Too many links",
	[EPIPE] = "Broken pipe",
	[EDOM] = "Numerical argument out of domain",
	[ERANGE] = "Numerical result out of range",
	[EAGAIN] = "Resource temporarily unavailable",
	[EINPROGRESS] = "Operation now in progress",
	[EALREADY] = "Operation already in progress",
	[ENOTSOCK] = "Socket operation on non-socket",
	[EDESTADDRREQ] = "Destination address required",
	[EMSGSIZE] = "Message too long",
	[EPROTOTYPE] = "Protocol wrong type for socket",
	[ENOPROTOOPT] = "Protocol not available",
	[EPROTONOSUPPORT] = "Protocol not supported",
	[ESOCKTNOSUPPORT] = "Socket type not supported",
	[EOPNOTSUPP] = "Operation not supported",
	[EPFNOSUPPORT] = "Protocol family not supported",
	[EAFNOSUPPORT] = "Address family not supported by protocol",
	[EADDRINUSE] = "Address already in use",
	[EADDRNOTAVAIL] = "Cannot assign requested address",
	[ENETDOWN] = "Network is down",
	[ENETUNREACH] = "Network is unreachable",
	[ENETRESET] = "Network dropped connection on reset",
	[ECONNABORTED] = "Software caused connection abort",
	[ECONNRESET] = "Connection reset by peer",
	[ENOBUFS] = "No buffer space available",
	[EISCONN] = "Transport endpoint is already connected",
	[ENOTCONN] = "Transport endpoint is not connected",
	[ESHUTDOWN] = "Cannot send after transport endpoint shutdown",
	[ETOOMANYREFS] = "Too many references: cannot splice",
	[ETIMEDOUT] = "Connection timed out",
	[ECONNREFUSED] = "Connection refused",
	[ELOOP] = "Too many levels of symbolic links",
	[ENAMETOOLONG] = "File name too long",
	[EHOSTDOWN] = "Host is down",
	[EHOSTUNREACH] = "No route to host",
	[ENOTEMPTY] = "Directory not empty",
	[EPROCLIM] = "Too many processes",
	[EUSERS] = "Too many users",
	[EDQUOT] = "Disk quota exceeded",
	[ESTALE] = "Stale file handle",
	[EREMOTE] = "Object is remote",
	[EBADRPC] = "RPC struct is bad",
	[ERPCMISMATCH] = "RPC version wrong",
	[EPROGUNAVAIL] = "RPC prog. not avail",
	[EPROGMISMATCH] = "Program version wrong",
	[EPROCUNAVAIL] = "Bad procedure for program",
	[ENOLCK] = "No locks available",
	[ENOSYS] = "Function not implemented",
	[EFTYPE] = "Inappropriate file type or format",
	[EAUTH] = "Authentication error",
	[ENEEDAUTH] = "Need authenticator",
	[EIDRM] = "Identifier removed",
	[ENOMSG] = "No message of desired type",
	[EOVERFLOW] = "Value too large for defined data type",
	[ECANCELED] = "Operation canceled",
	[EILSEQ] = "Invalid or incomplete multibyte or wide character",
	[ENOATTR] = "Attribute not found",
	[EDOOFUS] = "Programming error",
	[EBADMSG] = "Bad message",
	[EMULTIHOP] = "Multihop attempted",
	[ENOLINK] = "Link has been severed",
	[EPROTO] = "Protocol error",
	[ENOTCAPABLE] = "Capabilities insufficient",
	[ECAPMODE] = "Not permitted in capability mode",
	[ENOTRECOVERABLE] = "State not recoverable",
	[EOWNERDEAD] = "Previous owner died"
};

#define MAX_ERROR_NUM (sizeof(error_messages) / sizeof(error_messages[0]) - 1)

/*
 * Return string describing error number
 */
char *strerror(int errnum)
{
	if (errnum < 0 || errnum > MAX_ERROR_NUM || !error_messages[errnum]) {
		static char unknown_error[64];
		snprintf(unknown_error, sizeof(unknown_error), "Unknown error %d", errnum);
		return unknown_error;
	}
	
	return (char *)error_messages[errnum];
}

/*
 * Thread-safe version of strerror (BSD/POSIX)
 */
int strerror_r(int errnum, char *buf, size_t buflen)
{
	const char *msg;
	size_t len;
	
	if (!buf || buflen == 0) {
		return EINVAL;
	}
	
	if (errnum < 0 || errnum > MAX_ERROR_NUM || !error_messages[errnum]) {
		snprintf(buf, buflen, "Unknown error %d", errnum);
	} else {
		msg = error_messages[errnum];
		len = strlen(msg);
		
		if (len >= buflen) {
			// Message doesn't fit, truncate
			memcpy(buf, msg, buflen - 1);
			buf[buflen - 1] = '\0';
			return ERANGE;
		} else {
			strcpy(buf, msg);
		}
	}
	
	return 0;
}

/*
 * Print error message to stderr (BSD/POSIX)
 */
void perror(const char *s)
{
	if (s && *s) {
		fprintf(stderr, "%s: %s\n", s, strerror(errno));
	} else {
		fprintf(stderr, "%s\n", strerror(errno));
	}
}

/*
 * BSD-style warn functions
 */
void warn(const char *fmt, ...)
{
	va_list ap;
	
	if (fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(errno));
}

void warnx(const char *fmt, ...)
{
	va_list ap;
	
	if (fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	fprintf(stderr, "\n");
}

void vwarn(const char *fmt, va_list ap)
{
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(errno));
}

void vwarnx(const char *fmt, va_list ap)
{
	if (fmt) {
		vfprintf(stderr, fmt, ap);
	}
	fprintf(stderr, "\n");
}

/*
 * BSD-style err functions (these exit the program)
 */
void err(int eval, const char *fmt, ...)
{
	va_list ap;
	
	if (fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(errno));
	exit(eval);
}

void errx(int eval, const char *fmt, ...)
{
	va_list ap;
	
	if (fmt) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
	fprintf(stderr, "\n");
	exit(eval);
}

void verr(int eval, const char *fmt, va_list ap)
{
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(errno));
	exit(eval);
}

void verrx(int eval, const char *fmt, va_list ap)
{
	if (fmt) {
		vfprintf(stderr, fmt, ap);
	}
	fprintf(stderr, "\n");
	exit(eval);
}