#ifndef XPATH_H
#define XPATH_H

#include <stdint.h>

#define XP_NORMAL          0x1
#define XP_DIRECTORY       0x2
#define XP_HIDDEN          0x4
#define XP_READONLY        0x8
#define XP_SYSTEM          0x10
#define XP_EXECUTABLE      0x20

typedef struct {
    unsigned char *data;
    int count;
} xp_path;

typedef struct {
    uint32_t year;
    uint32_t month;
    uint32_t day;
    uint32_t hour;
    uint32_t minute;
    uint32_t second;
    uint32_t milliseconds;
} xp_time;

typedef struct {
    char *name;
    uint64_t bytes;
    uint32_t attributes;
    uint64_t time;
} xp_file;

typedef struct {
    xp_path path;
    xp_file *files;
    int file_count;
    // int file_cap;
} xp_directory;

xp_path xp_path_new(char *file_name);
void xp_path_free(xp_path *path);
bool xp_path_relative(xp_path path);

void xp_path_append(xp_path *path, char *str);
xp_path xp_substr(xp_path path, int start, int count);
void xp_append(xp_path *path, char *str);

xp_path xp_current_path();
xp_path xp_get_home_path();

xp_path xp_fullpath(xp_path path);
void xp_normalize(xp_path *path);
xp_path xp_path_copy(xp_path path);
void xp_replace_slashes(xp_path path);
xp_path xp_parent_path(xp_path path);

bool xp_path_is_dir(xp_path path);
bool xp_directory_new(xp_path path, xp_directory *directory);
void xp_directory_free(xp_directory *directory);

xp_time xp_utc_time(uint64_t time);

#endif // XPATH_H
