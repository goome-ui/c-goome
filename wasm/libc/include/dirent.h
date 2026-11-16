#ifndef LC_DIRENT_H
#define LC_DIRENT_H
typedef struct lc_dir DIR;
struct dirent {
    unsigned char d_type;
    char d_name[256];
};
#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8
#define DT_LNK 10
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#endif
