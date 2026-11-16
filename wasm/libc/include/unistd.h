#ifndef LC_UNISTD_H
#define LC_UNISTD_H
#include <sys/types.h>
ssize_t read(int fd, void *p, size_t n);
ssize_t write(int fd, const void *p, size_t n);
int close(int fd);
uid_t getuid(void);
int usleep(unsigned us);
#endif
