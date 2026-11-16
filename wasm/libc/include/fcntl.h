#ifndef LC_FCNTL_H
#define LC_FCNTL_H
#include <sys/types.h>
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_NONBLOCK 04000
#define O_CLOEXEC 02000000
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1
int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);
#endif
