#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "file.h"

FileData read_file(const char *path)
{
    FileData result = {NULL, 0};

    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return result;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *data = malloc(file_size);
    if (data == NULL)
    {
        fclose(file);
        return result;
    }

    fread(data, 1, file_size, file);
    fclose(file);

    result.data = data;
    result.size = file_size;

    return result;
}