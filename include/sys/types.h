/*
 * BSD-style sys/types.h - system types
 */

#ifndef _SYS_TYPES_H_
#define _SYS_TYPES_H_

#include <stdint.h>
#include <stddef.h>

/* BSD-style type definitions */
typedef uint8_t         u_int8_t;       /* unsigned 8-bit integer */
typedef uint16_t        u_int16_t;      /* unsigned 16-bit integer */
typedef uint32_t        u_int32_t;      /* unsigned 32-bit integer */
typedef uint64_t        u_int64_t;      /* unsigned 64-bit integer */

typedef uint8_t         u_char;         /* unsigned char */
typedef uint16_t        u_short;        /* unsigned short */
typedef uint32_t        u_int;          /* unsigned int */
typedef uint32_t        u_long;         /* unsigned long */

typedef int32_t         register_t;     /* register-sized type */

/* Process and user IDs */
typedef int32_t         pid_t;          /* process id */
typedef uint32_t        uid_t;          /* user id */
typedef uint32_t        gid_t;          /* group id */

/* File system types */
typedef int32_t         dev_t;          /* device number */
typedef uint32_t        ino_t;          /* inode number */
typedef uint16_t        mode_t;         /* file mode */
typedef uint16_t        nlink_t;        /* link count */
typedef int64_t         off_t;          /* file offset */
typedef int32_t         blksize_t;      /* block size */
typedef int64_t         blkcnt_t;       /* block count */

/* Time types */
typedef int64_t         time_t;         /* time in seconds since epoch */
typedef int32_t         suseconds_t;    /* microseconds */
typedef uint32_t        useconds_t;     /* microseconds (unsigned) */
typedef int32_t         clock_t;        /* clock ticks */

/* Memory and size types - ssize_t and size_t are defined in stddef.h */

/* BSD-specific types */
typedef uint32_t        in_addr_t;      /* IPv4 address */
typedef uint16_t        in_port_t;      /* port number */
typedef uint8_t         sa_family_t;    /* socket address family */
typedef uint32_t        socklen_t;      /* socket length type */

/* File descriptor type */
typedef int             fd_t;           /* file descriptor */

/* Key type for IPC */
typedef int32_t         key_t;          /* IPC key */

/* Priority type */
typedef int             pri_t;          /* priority */

/* CPU set types (for SMP support) */
typedef struct {
    uint32_t bits[8];                   /* supports up to 256 CPUs */
} cpuset_t;

/* BSD kqueue types */
typedef uint32_t        uintptr_t;      /* pointer-sized unsigned integer */
typedef int32_t         intptr_t;       /* pointer-sized signed integer */

/* Network byte order conversion (though these should be in arpa/inet.h) */
typedef uint16_t        n_short;        /* network short */
typedef uint32_t        n_long;         /* network long */
typedef uint32_t        n_time;         /* network time */

/* Resource limit types */
typedef uint64_t        rlim_t;         /* resource limit */

/* File system ID type */
typedef struct {
    int32_t val[2];
} fsid_t;

/* Fixed-size types for binary compatibility */
typedef int8_t          int8_t;
typedef int16_t         int16_t;
typedef int32_t         int32_t;
typedef int64_t         int64_t;

/* Quad types (legacy BSD) */
typedef int64_t         quad_t;         /* 64-bit signed */
typedef uint64_t        u_quad_t;       /* 64-bit unsigned */

/* Address types */
typedef uint32_t        vm_offset_t;    /* virtual memory offset */
typedef uint32_t        vm_size_t;      /* virtual memory size */
typedef uint32_t        vm_paddr_t;     /* physical address */

/* Boolean type is defined in stdbool.h */

#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

/* Endianness definitions */
#define LITTLE_ENDIAN   1234
#define BIG_ENDIAN      4321
#define PDP_ENDIAN      3412

#ifdef __i386__
#define BYTE_ORDER      LITTLE_ENDIAN
#else
#define BYTE_ORDER      LITTLE_ENDIAN  /* assume little endian for now */
#endif

#endif /* _SYS_TYPES_H_ */