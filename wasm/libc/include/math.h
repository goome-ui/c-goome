#ifndef LC_MATH_H
#define LC_MATH_H
#define M_PI 3.14159265358979323846
#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
__attribute__((import_module("env"), import_name("sin"))) double sin(double a);
__attribute__((import_module("env"), import_name("cos"))) double cos(double a);
__attribute__((import_module("env"), import_name("tan"))) double tan(double a);
__attribute__((import_module("env"), import_name("atan"))) double atan(double a);
__attribute__((import_module("env"), import_name("atan2"))) double atan2(double a, double b);
__attribute__((import_module("env"), import_name("asin"))) double asin(double a);
__attribute__((import_module("env"), import_name("acos"))) double acos(double a);
__attribute__((import_module("env"), import_name("exp"))) double exp(double a);
__attribute__((import_module("env"), import_name("log"))) double log(double a);
__attribute__((import_module("env"), import_name("log2"))) double log2(double a);
__attribute__((import_module("env"), import_name("log10"))) double log10(double a);
__attribute__((import_module("env"), import_name("pow"))) double pow(double a, double b);
__attribute__((import_module("env"), import_name("sinh"))) double sinh(double a);
__attribute__((import_module("env"), import_name("cosh"))) double cosh(double a);
__attribute__((import_module("env"), import_name("tanh"))) double tanh(double a);
static inline double sqrt(double a) { return __builtin_sqrt(a); }
static inline double fabs(double a) { return __builtin_fabs(a); }
static inline double floor(double a) { return __builtin_floor(a); }
static inline double ceil(double a) { return __builtin_ceil(a); }
static inline double trunc(double a) { return __builtin_trunc(a); }
static inline double round(double a) { return a < 0 ? __builtin_ceil(a - 0.5) : __builtin_floor(a + 0.5); }
static inline double fmod(double a, double b) { return a - __builtin_trunc(a / b) * b; }
static inline double fmin(double a, double b) { return a < b ? a : b; }
static inline double fmax(double a, double b) { return a > b ? a : b; }
static inline float sqrtf(float a) { return __builtin_sqrtf(a); }
static inline float fabsf(float a) { return __builtin_fabsf(a); }
static inline float floorf(float a) { return __builtin_floorf(a); }
static inline float ceilf(float a) { return __builtin_ceilf(a); }
static inline float sinf(float a) { return (float)sin(a); }
static inline float cosf(float a) { return (float)cos(a); }
static inline float powf(float a, float b) { return (float)pow(a, b); }
static inline float atan2f(float a, float b) { return (float)atan2(a, b); }
#endif
