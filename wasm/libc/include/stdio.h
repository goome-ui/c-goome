#ifndef LC_STDIO_H
#define LC_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct lc_file FILE;
extern FILE *stdin, *stdout, *stderr;
#define EOF (-1)
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t sz, size_t n, FILE *f);
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f);
int ferror(FILE *f);
int fgetc(FILE *f);
int fputs(const char *s, FILE *f);
int puts(const char *s);
int fputc(int c, FILE *f);
int putchar(int c);
int fflush(FILE *f);
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
#endif
