#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <dlfcn.h>
#include <dirent.h>

int errno;
FILE *stdin, *stdout, *stderr;

extern unsigned char __heap_base;

typedef struct lc_blk {
    size_t size;
    struct lc_blk *next;
} lc_blk;

#define LC_HDR 16
#define LC_CLASSES 40

static uintptr_t lc_top, lc_end;
static lc_blk *lc_free[LC_CLASSES];

static int lc_class(size_t n, size_t *rounded)
{
    size_t s = 16;
    int k = 0;
    while (s < n) s <<= 1, k++;
    *rounded = s;
    return k;
}

static int lc_more(size_t need)
{
    if (!lc_top) lc_top = ((uintptr_t)&__heap_base + 15) & ~(uintptr_t)15, lc_end = (uintptr_t)__builtin_wasm_memory_size(0) * 65536;
    while (lc_end - lc_top < need) {
        size_t pages = (need - (lc_end - lc_top) + 65535) / 65536;
        if (pages < 16) pages = 16;
        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1) return 0;
        lc_end += pages * 65536;
    }
    return 1;
}

void *malloc(size_t n)
{
    size_t sz;
    if (n > ((size_t)1 << 30)) return NULL;
    int k = lc_class(n ? n : 1, &sz);
    lc_blk *b = lc_free[k];
    if (b) {
        lc_free[k] = b->next;
        return (char *)b + LC_HDR;
    }
    if (!lc_more(sz + LC_HDR)) return NULL;
    b = (lc_blk *)lc_top;
    lc_top += sz + LC_HDR;
    b->size = sz;
    return (char *)b + LC_HDR;
}

void free(void *p)
{
    if (!p) return;
    lc_blk *b = (lc_blk *)((char *)p - LC_HDR);
    size_t sz;
    int k = lc_class(b->size, &sz);
    b->next = lc_free[k];
    lc_free[k] = b;
}

void *calloc(size_t k, size_t n)
{
    if (n && k > ((size_t)1 << 30) / n) return NULL;
    void *p = malloc(k * n);
    if (p) memset(p, 0, k * n);
    return p;
}

void *realloc(void *p, size_t n)
{
    if (!p) return malloc(n);
    lc_blk *b = (lc_blk *)((char *)p - LC_HDR);
    if (n <= b->size) return p;
    void *q = malloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->size);
    free(p);
    return q;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *a = d;
    const unsigned char *b = s;
    while (n >= 4 && !(((uintptr_t)a | (uintptr_t)b) & 3)) {
        *(uint32_t *)a = *(const uint32_t *)b;
        a += 4, b += 4, n -= 4;
    }
    while (n--) *a++ = *b++;
    return d;
}

void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *a = d;
    const unsigned char *b = s;
    if (a == b || !n) return d;
    if (a < b || a >= b + n) return memcpy(d, s, n);
    while (n--) a[n] = b[n];
    return d;
}

void *memset(void *d, int c, size_t n)
{
    unsigned char *a = d;
    uint32_t w = (unsigned char)c * 0x01010101u;
    while (n && ((uintptr_t)a & 3)) *a++ = (unsigned char)c, n--;
    while (n >= 4) *(uint32_t *)a = w, a += 4, n -= 4;
    while (n--) *a++ = (unsigned char)c;
    return d;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return *x - *y;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c) return (void *)p;
    return NULL;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

char *strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++)) {}
    return r;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (!*a) return 0;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return NULL;
    }
}

char *strrchr(const char *s, int c)
{
    const char *r = NULL;
    for (;; s++) {
        if (*s == (char)c) r = s;
        if (!*s) return (char *)r;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t k = strlen(n);
    if (!k) return (char *)h;
    for (; *h; h++)
        if (*h == *n && !strncmp(h, n, k)) return (char *)h;
    return NULL;
}

int abs(int v) { return v < 0 ? -v : v; }
void abort(void) { __builtin_trap(); }
char *getenv(const char *name) { return NULL; }

FILE *fopen(const char *path, const char *mode) { return NULL; }
int fclose(FILE *f) { return EOF; }
size_t fread(void *p, size_t sz, size_t n, FILE *f) { return 0; }
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f) { return 0; }
int ferror(FILE *f) { return 1; }
int fgetc(FILE *f) { return EOF; }
int fputs(const char *s, FILE *f) { return EOF; }
int puts(const char *s) { return EOF; }
int stat(const char *path, struct stat *st) { return -1; }
int lstat(const char *path, struct stat *st) { return -1; }
int fstat(int fd, struct stat *st) { return -1; }
int mkdir(const char *path, mode_t mode) { return -1; }
int mkfifo(const char *path, mode_t mode) { return -1; }
int open(const char *path, int flags, ...) { return -1; }
int fcntl(int fd, int cmd, ...) { return -1; }
ssize_t read(int fd, void *p, size_t n) { return -1; }
ssize_t write(int fd, const void *p, size_t n) { return -1; }
int close(int fd) { return -1; }
uid_t getuid(void) { return 0; }
int usleep(unsigned us) { return 0; }
void *dlopen(const char *name, int flags) { return NULL; }
void *dlsym(void *lib, const char *name) { return NULL; }
int dlclose(void *lib) { return 0; }
DIR *opendir(const char *path) { return NULL; }
struct dirent *readdir(DIR *d) { return NULL; }
int closedir(DIR *d) { return 0; }

static double lc_now_ms;

void lc_set_time(double ms) { lc_now_ms = ms; }

int clock_gettime(int clk, struct timespec *t)
{
    long long us = (long long)(lc_now_ms * 1000.0);
    t->tv_sec = (time_t)(us / 1000000);
    t->tv_nsec = (long)(us % 1000000) * 1000;
    return 0;
}

int nanosleep(const struct timespec *t, struct timespec *left)
{
    lc_now_ms += (double)t->tv_sec * 1000.0 + t->tv_nsec / 1000000.0;
    return 0;
}

typedef struct {
    char *buf;
    size_t cap, len;
} lc_out;

static void lc_put(lc_out *o, char c)
{
    if (o->len + 1 < o->cap) o->buf[o->len] = c;
    o->len++;
}

static void lc_puts(lc_out *o, const char *s, size_t n)
{
    while (n--) lc_put(o, *s++);
}

static void lc_pad(lc_out *o, char c, int n)
{
    while (n-- > 0) lc_put(o, c);
}

#define LC_LIMBS 140
#define LC_BASE 1000000000u

static int lc_exact(double v, char *dig, int *pt)
{
    union {
        double d;
        uint64_t u;
    } x = {v};
    int e = (int)((x.u >> 52) & 0x7FF);
    uint64_t m = x.u & (((uint64_t)1 << 52) - 1);
    if (!e && !m) {
        dig[0] = '0';
        *pt = 1;
        return 1;
    }
    if (e) m |= (uint64_t)1 << 52;
    else e = 1;
    int e2 = e - 1075;
    uint32_t big[LC_LIMBS];
    int n = 0;
    while (m) big[n++] = (uint32_t)(m % LC_BASE), m /= LC_BASE;
    int k = e2 < 0 ? -e2 : 0, twos = e2 > 0 ? e2 : 0;
    while (k > 0 || twos > 0) {
        uint64_t mul;
        if (k > 0) {
            int s = k > 13 ? 13 : k;
            mul = 1;
            for (int i = 0; i < s; i++) mul *= 5;
            k -= s;
        } else {
            int s = twos > 29 ? 29 : twos;
            mul = (uint64_t)1 << s;
            twos -= s;
        }
        uint64_t carry = 0;
        for (int i = 0; i < n; i++) {
            uint64_t t = (uint64_t)big[i] * mul + carry;
            big[i] = (uint32_t)(t % LC_BASE);
            carry = t / LC_BASE;
        }
        while (carry) big[n++] = (uint32_t)(carry % LC_BASE), carry /= LC_BASE;
    }
    int len = 0;
    char tmp[16];
    for (int i = n - 1; i >= 0; i--) {
        uint32_t w = big[i];
        for (int j = 8; j >= 0; j--) tmp[j] = (char)('0' + w % 10), w /= 10;
        int j = 0;
        if (i == n - 1)
            while (j < 8 && tmp[j] == '0') j++;
        for (; j < 9; j++) dig[len++] = tmp[j];
    }
    *pt = len - (e2 < 0 ? -e2 : 0);
    while (len > 1 && dig[len - 1] == '0') len--;
    return len;
}

static int lc_round(char *dig, int len, int *pt, int keep)
{
    if (keep >= len) return len;
    if (keep < 0) {
        dig[0] = '0';
        *pt = 1;
        return 1;
    }
    int up = 0;
    if (dig[keep] > '5') up = 1;
    else if (dig[keep] == '5') {
        int rest = 0;
        for (int i = keep + 1; i < len; i++)
            if (dig[i] != '0') rest = 1;
        up = rest || (keep > 0 && ((dig[keep - 1] - '0') & 1));
    }
    len = keep;
    if (up) {
        int i = keep - 1;
        while (i >= 0 && dig[i] == '9') dig[i--] = '0';
        if (i < 0) {
            memmove(dig + 1, dig, (size_t)len);
            dig[0] = '1';
            len++;
            (*pt)++;
        } else dig[i]++;
    }
    if (!len) {
        dig[0] = '0';
        *pt = 1;
        return 1;
    }
    while (len > 1 && dig[len - 1] == '0') len--;
    return len;
}

static int lc_fmt_float(char *out, double v, int prec, char conv, int alt)
{
    int o = 0, upper = conv == 'F' || conv == 'E' || conv == 'G';
    char c = (char)(conv | 32);
    if (v != v || v - v != 0) {
        const char *s = v != v ? (upper ? "NAN" : "nan") : (upper ? "INF" : "inf");
        memcpy(out, s, 3);
        return 3;
    }
    static char dig[1200];
    int pt, len = lc_exact(v, dig, &pt);
    int zero = len == 1 && dig[0] == '0';
    if (prec < 0) prec = 6;
    int strip = 0;
    if (c == 'g') {
        int p = prec ? prec : 1;
        if (!zero) len = lc_round(dig, len, &pt, p);
        int x = zero ? 0 : pt - 1;
        if (x < p && x >= -4) c = 'f', prec = p - 1 - x;
        else c = 'e', prec = p - 1;
        strip = !alt;
    }
    if (c == 'f') {
        if (!zero) {
            len = lc_round(dig, len, &pt, pt + prec);
            zero = len == 1 && dig[0] == '0';
            if (zero) pt = 1;
        }
        if (pt <= 0 || zero) out[o++] = '0';
        else
            for (int i = 0; i < pt; i++) out[o++] = i < len ? dig[i] : '0';
        int fs = o;
        if (prec > 0 || alt) out[o++] = '.';
        for (int i = 0; i < prec; i++) {
            int k = pt + i;
            out[o++] = zero || k < 0 || k >= len ? '0' : dig[k];
        }
        if (strip && prec > 0) {
            while (o > fs + 1 && out[o - 1] == '0') o--;
            if (o == fs + 1) o = fs;
        }
        return o;
    }
    if (!zero) len = lc_round(dig, len, &pt, prec + 1);
    int x = zero ? 0 : pt - 1;
    out[o++] = dig[0];
    int fs = o;
    if (prec > 0 || alt) out[o++] = '.';
    for (int i = 1; i <= prec; i++) out[o++] = i < len && !zero ? dig[i] : '0';
    if (strip && prec > 0) {
        while (o > fs + 1 && out[o - 1] == '0') o--;
        if (o == fs + 1) o = fs;
    }
    out[o++] = upper ? 'E' : 'e';
    out[o++] = x < 0 ? '-' : '+';
    if (x < 0) x = -x;
    char eb[8];
    int n = 0;
    do eb[n++] = (char)('0' + x % 10), x /= 10;
    while (x);
    if (n < 2) eb[n++] = '0';
    while (n) out[o++] = eb[--n];
    return o;
}

int vsnprintf(char *buf, size_t cap, const char *f, va_list ap)
{
    lc_out o = {buf, cap, 0};
    static char tmp[1400];
    for (; *f; f++) {
        if (*f != '%') {
            lc_put(&o, *f);
            continue;
        }
        f++;
        int left = 0, plus = 0, space = 0, alt = 0, zpad = 0, width = 0, prec = -1, lng = 0;
        for (;; f++) {
            if (*f == '-') left = 1;
            else if (*f == '+') plus = 1;
            else if (*f == ' ') space = 1;
            else if (*f == '#') alt = 1;
            else if (*f == '0') zpad = 1;
            else break;
        }
        if (*f == '*') {
            width = va_arg(ap, int), f++;
            if (width < 0) left = 1, width = -width;
        } else
            while (*f >= '0' && *f <= '9') width = width * 10 + *f++ - '0';
        if (*f == '.') {
            f++, prec = 0;
            if (*f == '*') {
                prec = va_arg(ap, int), f++;
                if (prec < 0) prec = -1;
            } else
                while (*f >= '0' && *f <= '9') prec = prec * 10 + *f++ - '0';
        }
        for (;; f++) {
            if (*f == 'l') lng++;
            else if (*f == 'z' || *f == 'j' || *f == 't') lng = 2;
            else if (*f == 'h' || *f == 'L') {
            } else break;
        }
        char conv = *f;
        if (!conv) break;
        const char *s = tmp;
        int n = 0, neg = 0;
        char sign = 0;
        switch (conv) {
        case '%': lc_put(&o, '%'); continue;
        case 'c': tmp[0] = (char)va_arg(ap, int), n = 1, zpad = 0; break;
        case 's': {
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while ((prec < 0 || n < prec) && s[n]) n++;
            zpad = 0;
            break;
        }
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'p': {
            unsigned long long v;
            if (conv == 'p') v = (uintptr_t)va_arg(ap, void *), alt = 1, conv = 'x';
            else if (conv == 'd' || conv == 'i') {
                long long sv = lng >= 2 ? va_arg(ap, long long) : lng ? va_arg(ap, long) : va_arg(ap, int);
                if (sv < 0) neg = 1, v = 0ull - (unsigned long long)sv;
                else v = (unsigned long long)sv;
            } else
                v = lng >= 2 ? va_arg(ap, unsigned long long) : lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            unsigned base = conv == 'x' || conv == 'X' ? 16 : conv == 'o' ? 8 : 10;
            const char *dg = conv == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char r[32];
            int k = 0;
            while (v) r[k++] = dg[v % base], v /= base;
            int digits = prec < 0 ? (k ? k : 1) : k > prec ? k : prec;
            if (!k && prec == 0) digits = 0;
            if (alt && base == 16 && k) tmp[n++] = '0', tmp[n++] = conv == 'X' ? 'X' : 'x';
            for (int i = digits; i > k; i--) tmp[n++] = '0';
            while (k) tmp[n++] = r[--k];
            if (alt && base == 8 && (n == 0 || tmp[0] != '0')) {
                memmove(tmp + 1, tmp, (size_t)n);
                tmp[0] = '0', n++;
            }
            if (prec >= 0) zpad = 0;
            if (conv == 'd' || conv == 'i') sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': {
            double v = va_arg(ap, double);
            if (v < 0 || (v == 0 && 1 / v < 0)) neg = 1, v = -v;
            n = lc_fmt_float(tmp, v, prec, conv, alt);
            if (v != v || v - v != 0) zpad = 0;
            sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
            break;
        }
        default:
            lc_put(&o, '%');
            lc_put(&o, conv);
            continue;
        }
        int total = n + (sign ? 1 : 0);
        if (!left && !zpad) lc_pad(&o, ' ', width - total);
        if (sign) lc_put(&o, sign);
        if (!left && zpad) {
            int skip = 0;
            if (n >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) lc_puts(&o, s, 2), skip = 2;
            lc_pad(&o, '0', width - total);
            lc_puts(&o, s + skip, (size_t)(n - skip));
        } else lc_puts(&o, s, (size_t)n);
        if (left) lc_pad(&o, ' ', width - total);
    }
    if (cap) buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) { return 0; }

int fprintf(FILE *f, const char *fmt, ...) { return 0; }

int printf(const char *fmt, ...) { return 0; }
