#ifndef LC_STDLIB_H
#define LC_STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t k, size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
char *getenv(const char *name);
void abort(void);
int abs(int v);
#endif
