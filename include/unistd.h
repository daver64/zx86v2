/*
 * BSD-style unistd.h - standard symbolic constants and types
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <stddef.h>
#include <sys/types.h>

/* Standard file descriptors */
#define STDIN_FILENO    0       /* Standard input */
#define STDOUT_FILENO   1       /* Standard output */
#define STDERR_FILENO   2       /* Standard error */

/* Values for the second argument to access() */
#define F_OK            0       /* Test for existence */
#define X_OK            0x01    /* Test for execute permission */
#define W_OK            0x02    /* Test for write permission */
#define R_OK            0x04    /* Test for read permission */

/* Values for the third argument to lseek() */
#define SEEK_SET        0       /* Set file pointer to offset */
#define SEEK_CUR        1       /* Set file pointer to current plus offset */
#define SEEK_END        2       /* Set file pointer to EOF plus offset */

/* Compile-time symbolic constants */
#define _POSIX_VERSION          200809L
#define _POSIX2_VERSION         200809L

/* Run-time invariant values */
#define _SC_ARG_MAX             1
#define _SC_CHILD_MAX           2
#define _SC_CLK_TCK             3
#define _SC_NGROUPS_MAX         4
#define _SC_OPEN_MAX            5
#define _SC_STREAM_MAX          6
#define _SC_TZNAME_MAX          7
#define _SC_JOB_CONTROL         8
#define _SC_SAVED_IDS           9
#define _SC_REALTIME_SIGNALS    10
#define _SC_VERSION             11
#define _SC_BC_BASE_MAX         12
#define _SC_BC_DIM_MAX          13
#define _SC_BC_SCALE_MAX        14
#define _SC_BC_STRING_MAX       15
#define _SC_COLL_WEIGHTS_MAX    16
#define _SC_EXPR_NEST_MAX       17
#define _SC_LINE_MAX            18
#define _SC_RE_DUP_MAX          19
#define _SC_2_VERSION           20
#define _SC_2_C_BIND            21
#define _SC_2_C_DEV             22
#define _SC_2_CHAR_TERM         23
#define _SC_2_FORT_DEV          24
#define _SC_2_FORT_RUN          25
#define _SC_2_LOCALEDEF         26
#define _SC_2_SW_DEV            27
#define _SC_2_UPE               28
#define _SC_PAGESIZE            29
#define _SC_PAGE_SIZE           _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN    30

/* Pathname variable values */
#define _PC_LINK_MAX            1
#define _PC_MAX_CANON           2
#define _PC_MAX_INPUT           3
#define _PC_NAME_MAX            4
#define _PC_PATH_MAX            5
#define _PC_PIPE_BUF            6
#define _PC_CHOWN_RESTRICTED    7
#define _PC_NO_TRUNC            8
#define _PC_VDISABLE            9

/* Function declarations */
#ifdef __cplusplus
extern "C" {
#endif

/* Process control */
pid_t   getpid(void);
pid_t   getppid(void);
pid_t   getpgrp(void);
int     setpgid(pid_t pid, pid_t pgid);
pid_t   setsid(void);

uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
gid_t   getegid(void);
int     setuid(uid_t uid);
int     seteuid(uid_t euid);
int     setgid(gid_t gid);
int     setegid(gid_t egid);

/* File operations */
int     access(const char *path, int amode);
int     chdir(const char *path);
int     fchdir(int fd);
char   *getcwd(char *buf, size_t size);
int     link(const char *path1, const char *path2);
int     unlink(const char *path);
int     rmdir(const char *path);

/* File descriptor operations */
int     close(int fd);
int     dup(int fd);
int     dup2(int fd, int fd2);
int     pipe(int fildes[2]);
ssize_t read(int fd, void *buf, size_t nbyte);
ssize_t write(int fd, const void *buf, size_t nbyte);
off_t   lseek(int fd, off_t offset, int whence);

/* System configuration */
long    sysconf(int name);
long    pathconf(const char *path, int name);
long    fpathconf(int fd, int name);

/* Process execution */
int     execl(const char *path, const char *arg, ...);
int     execle(const char *path, const char *arg, ...);
int     execlp(const char *file, const char *arg, ...);
int     execv(const char *path, char *const argv[]);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execvp(const char *file, char *const argv[]);

/* Process creation */
pid_t   fork(void);
pid_t   vfork(void);

/* Program termination */
void    _exit(int status);

/* Sleep functions */
unsigned int sleep(unsigned int seconds);
int     usleep(useconds_t useconds);

/* Alarm functions */
unsigned int alarm(unsigned int seconds);
int     pause(void);

/* Working directory */
char   *getlogin(void);
int     gethostname(char *name, size_t namelen);

/* Miscellaneous */
int     isatty(int fd);
char   *ttyname(int fd);
int     ttyname_r(int fd, char *buf, size_t buflen);

/* Environment */
extern char **environ;

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H_ */