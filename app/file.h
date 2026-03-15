#ifndef FILE_H
#define FILE_H

#include <stddef.h>

typedef struct
{
    char *data;
    size_t size;
} FileData;

FileData read_file(const char *path);
#endif // FILE_H