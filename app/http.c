#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define HEADER_END_LEN 4
#define HEADER_END_OVERLAP (HEADER_END_LEN - 1)

#define BUFFER_NULL_MARGIN 1
#define INFINITY_LOOP 1

#define LINE_END "\r\n"
#define LINE_END_LEN 2

#define OK 0
#define PARSE_ERROR -1
#define INVALID_HEADER -2

#define BUFFER_SIZE 1024

typedef struct
{
    char *data;
    size_t size;
} FileData;

typedef struct
{
    char method[8];
    char path[256];
    ssize_t content_length;
} HttpRequest;

/* =========================
   Function Declarations
========================= */

FileData read_file(const char *path);
const char *get_content_type(const char *path);
void build_file_path(const char *request_path, char *full_path, size_t size);
ssize_t read_http_header(int fd, char *buffer, size_t buffer_size, ssize_t *header_length);
int parse_http_request(const char *header, HttpRequest *request);
void handle_client(int client_fd);
void singchld_handler(int sig);
void handle_get(int client_fd, const char *request_path, const char *content_type);
ssize_t get_content_length(const char *header);
void handle_post(int client_fd, HttpRequest req, char *buffer, ssize_t header_size, ssize_t request_size);
void send_response(int client_fd, const char *status, const char *content_type, const void *body, const size_t body_size);

/* =========================
   File Handling
========================= */

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

const char *get_content_type(const char *path)
{
    const char *ext = strrchr(path, '.');

    if (ext == NULL)
        return "text/html";

    if (strcmp(ext, ".html") == 0)
        return "text/html";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".js") == 0)
        return "application/javascript";
    if (strcmp(ext, ".json") == 0)
        return "application/json";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".ico") == 0)
        return "image/png";
    if (strcmp(ext, ".jpg") == 0 ||
        strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";

    return "text/plain";
}

/* =========================
   Path Handling
========================= */

void build_file_path(const char *request_path, char *full_path, size_t size)
{
    const char *base = "/var/www/html";

    if (strstr(request_path, "..") != NULL)
    {
        full_path[0] = '\0';
        return;
    }

    if (strcmp(request_path, "/") == 0)
    {
        snprintf(full_path, size, "%s/index.html", base);

        FILE *file = fopen(full_path, "r");
        if (file != NULL)
        {
            fclose(file);
            return;
        }

        snprintf(full_path, size, "%s/http-server.html", base);
        return;
    }

    snprintf(full_path, size, "%s%s", base, request_path);
}

/* =========================
   HTTP Header Read
========================= */

ssize_t read_http_header(int fd, char *buffer, size_t buffer_size, ssize_t *header_length)
{
    ssize_t total = 0;

    while (total < (ssize_t)(buffer_size - BUFFER_NULL_MARGIN))
    {
        ssize_t n = read(fd,
                         buffer + total,
                         buffer_size - BUFFER_NULL_MARGIN - total);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            perror("read error");
            return -1;
        }

        if (n == 0)
            return -1;

        total += n;

        ssize_t start = total - n - HEADER_END_OVERLAP;
        if (start < 0)
            start = 0;

        for (ssize_t i = start;
             i + HEADER_END_LEN <= total;
             i++)
        {
            if (buffer[i] == '\r' &&
                buffer[i + 1] == '\n' &&
                buffer[i + 2] == '\r' &&
                buffer[i + 3] == '\n')
            {
                buffer[total] = '\0';
                *header_length = i + HEADER_END_LEN;
                return total;
            }
        }
    }

    return -1;
}

/* =========================
   HTTP Parsing
========================= */

int parse_http_request(const char *header, HttpRequest *request)
{
    char method[8];
    char type[16];
    char path[256];

    if (sscanf(header, "%7s %255s", method, path) != 2)
        return PARSE_ERROR;

    strcpy(request->method, method);
    strcpy(request->path, path);

    request->content_length = get_content_length(header);

    return OK;
}

/* =========================
   Client Handling
========================= */

void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
    ssize_t header_size;

    printf("handle_client started, fd=%d, pid=%d\n",
           client_fd, getpid());
    fflush(stdout);

    ssize_t request_size =
        read_http_header(client_fd,
                         buffer,
                         sizeof(buffer),
                         &header_size);

    if (request_size < 0)
    {
        printf("ERROR: read_http_header failed, fd=%d\n",
               client_fd);
        fflush(stdout);
        return;
    }

    printf("REQUEST:\n%.*s\n",
           (int)header_size,
           buffer);
    fflush(stdout);

    HttpRequest req;

    if (parse_http_request(buffer, &req) == PARSE_ERROR)
    {
        int len = snprintf(buffer, sizeof(buffer),
                           "HTTP/1.1 400 Bad Request\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n");

        write(client_fd, buffer, len);
        return;
    }

    char file_path[512];

    build_file_path(req.path, file_path, sizeof(file_path));

    const char *content_type = get_content_type(file_path);

    if (strcmp(req.method, "GET") == 0)
    {
        handle_get(client_fd, req.path, content_type);
    }
    else if (strcmp(req.method, "POST") == 0)
    {
        handle_post(client_fd, req,
                    buffer,
                    header_size,
                    request_size);
    }
    else
    {
        int len = snprintf(buffer, sizeof(buffer),
                           "HTTP/1.1 405 Method Not Allowed\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n");

        write(client_fd, buffer, len);
    }
}

/* =========================
   GET Handling
========================= */

void handle_get(int client_fd, const char *request_path, const char *content_type)
{
    char file_path[512];

    build_file_path(request_path,
                    file_path,
                    sizeof(file_path));

    FileData file_data = read_file(file_path);

    const char *status;

    if (file_data.data == NULL)
    {
        printf("error: read content\n");
        status = "404 Not Found";
    }
    else
    {
        status = "200 OK";
    }

    send_response(client_fd,
                  status,
                  content_type,
                  file_data.data,
                  file_data.size);

    free(file_data.data);
}

/* =========================
   POST Handling
========================= */

void handle_post(int client_fd, HttpRequest req, char *buffer, ssize_t header_size, ssize_t request_size)
{
    if (req.content_length < 0)
    {
        int len = snprintf(buffer, BUFFER_SIZE,
                           "HTTP/1.1 411 Length Required\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n");

        write(client_fd, buffer, len);
        return;
    }

    char *body_buffer = malloc(req.content_length + BUFFER_NULL_MARGIN);

    if (body_buffer == NULL)
        return;

    char *body_start_addr = buffer + header_size;

    ssize_t already_read_body_size = request_size - header_size;

    if (already_read_body_size > 0)
    {
        memcpy(body_buffer, body_start_addr, already_read_body_size);
    }

    ssize_t body_readed = already_read_body_size;

    while (body_readed < req.content_length)
    {
        ssize_t tn = read(client_fd, body_buffer + body_readed, req.content_length - body_readed);

        if (tn < 0)
        {
            if (errno == EINTR)
                continue;

            perror("read error");
            free(body_buffer);
            return;
        }

        if (tn == 0)
        {
            printf("error: body read cut\n");
            free(body_buffer);
            return;
        }

        body_readed += tn;
    }

    printf("POST body received, length=%zu\n", req.content_length);

    printf("body content:\n%.*s\n", (int)req.content_length, body_buffer);

    free(body_buffer);

    send_response(client_fd,
                  "200 OK",
                  "text/plain",
                  "POST received",
                  strlen("POST received"));
}

void handle_php(int client_fd, HttpRequest req, char *buffer, ssize_t header_size, ssize_t request_size)
{
    //
}

/* =========================
   Response Sending
========================= */

void send_response(int client_fd, const char *status, const char *content_type, const void *body, const size_t body_size)
{
    char header[512];

    if (content_type == NULL)
        content_type = "text/plain";

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status,
                              content_type,
                              body_size);

    ssize_t header_written = 0;

    while (header_written < header_len)
    {
        ssize_t written =
            write(client_fd,
                  header + header_written,
                  header_len - header_written);

        if (written <= 0)
        {
            perror("write failed");
            return;
        }

        header_written += written;
    }

    const char *p = body;
    ssize_t body_written = 0;

    while (body_written < (ssize_t)body_size)
    {
        ssize_t written =
            write(client_fd,
                  p + body_written,
                  body_size - body_written);

        if (written <= 0)
        {
            perror("write failed");
            return;
        }

        body_written += written;
    }
}

/* =========================
   Content-Length Parsing
========================= */

ssize_t get_content_length(const char *header)
{
    const char *line_start = header;

    const char *CONTENT_LENGTH_PREFIX =
        "Content-Length:";

    const size_t CONTENT_LENGTH_COUNT =
        strlen(CONTENT_LENGTH_PREFIX);

    while (INFINITY_LOOP)
    {
        const char *line_end =
            strstr(line_start, LINE_END);

        if (line_end == NULL)
            break;

        if (line_end == line_start)
            break;

        size_t line_length =
            line_end - line_start;

        if (line_length < CONTENT_LENGTH_COUNT)
        {
            line_start =
                line_end + LINE_END_LEN;
            continue;
        }

        if (strncasecmp(line_start,
                        CONTENT_LENGTH_PREFIX,
                        CONTENT_LENGTH_COUNT) == 0)
        {
            const char *line_ptr =
                line_start + CONTENT_LENGTH_COUNT;

            while (line_ptr < line_end &&
                   *line_ptr == ' ')
            {
                line_ptr++;
            }

            return strtol(line_ptr,
                          NULL,
                          10);
        }

        line_start =
            line_end + LINE_END_LEN;
    }

    return -1;
}
