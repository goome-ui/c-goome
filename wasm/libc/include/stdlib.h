#ifndef LC_STDLIB_H
#define LC_STDLIB_H
#include <stddef.h>
#define RAND_MAX 32767
void *malloc(size_t n);
void *calloc(size_t k, size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
char *getenv(const char *name);
void abort(void);
int abs(int v);
long strtol(const char *s, char **end, int base);
double strtod(const char *s, char **end);
int atoi(const char *s);
double atof(const char *s);
int rand(void);
void srand(unsigned s);
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
#endif
