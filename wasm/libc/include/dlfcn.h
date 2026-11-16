#ifndef LC_DLFCN_H
#define LC_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 256
#define RTLD_DEFAULT ((void *)0)
void *dlopen(const char *name, int flags);
void *dlsym(void *lib, const char *name);
int dlclose(void *lib);
#endif
