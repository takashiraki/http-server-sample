#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

typedef struct
{
    char *data;
    size_t size;
} FileData;

void handle_client(int client_fd);

#endif // HTTP_H