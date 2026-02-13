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

// \r\n\r\n の跨ぎ検出のため3byte戻る
#define HEADER_END_OVERLAP (HEADER_END_LEN - 1)

#define BUFFER_NULL_MARGIN 1
#define INFINITY_LOOP 1
#define LINE_END "\r\n"
#define LINE_END_LEN 2

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
    char content_type[64];
} HttpRequest;

FileData read_file(const char *path);

const char *get_content_type(const char *path);

void build_file_path(const char *request_path, char *full_path, size_t size);

// socket(fd) → memory(buffer)
ssize_t read_http_header(int fd, char *buffer, size_t buffer_size, ssize_t *header_length);

void handle_client(int client_fd);

void singchld_handler(int sig);

void handle_get(int client_fd, const char *request_path);

ssize_t get_content_length(const char *header);

void send_response(int client_fd, const char *status, const char *content_type, const void *body, const size_t body_size);

FileData read_file(const char *path)
{
    // ファイルを取得
    FileData result = {NULL, 0};

    FILE *file = fopen(path, "rb");

    if (file == NULL)
        return result;

    // ファイルサイズを取得
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // メモリ確保
    char *data = malloc(file_size);

    if (data == NULL)
    {
        fclose(file);
        return result;
    }

    // ファイルの読み込み
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
    {
        return "text/html";
    }

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
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";

    return "text/plain";
}

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

// socket(fd) → memory(buffer)
// あくまでソケットから来た内容をメモリに移すだけ
ssize_t read_http_header(int fd, char *buffer, size_t buffer_size, ssize_t *header_length)
{
    ssize_t total = 0;
    ssize_t n;

    while (total < (ssize_t)(buffer_size - BUFFER_NULL_MARGIN))
    {
        n = read(fd, buffer + total, buffer_size - BUFFER_NULL_MARGIN - total);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            perror("read error");
            return -1;
        }

        if (n == 0)
        {
            // 途中で切れた
            return -1;
        }

        total += n;

        ssize_t start = total - n - HEADER_END_OVERLAP;
        if (start < 0)
        {
            start = 0;
        }

        for (ssize_t i = start; i + HEADER_END_LEN <= total; i++)
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

    // ヘッダー終端が見つからなかった
    // or バッファオーバーフロー
    return -1;
}

void handle_client(int client_fd)
{
    char buffer[1024];
    ssize_t header_size;

    printf("handle_client started, fd=%d, pid=%d\n", client_fd, getpid());
    fflush(stdout);

    ssize_t request_size = read_http_header(client_fd, buffer, sizeof(buffer), &header_size);

    if (request_size < 0)
    {
        // 読み込みエラー
        printf("ERROR: read_http_header failed, fd=%d\n", client_fd);
        fflush(stdout);
        return;
    }

    printf("REQUEST:\n%.*s\n", (int)header_size, buffer);
    fflush(stdout);

    // ファイル読み込み
    char request_path[256];

    // メソッド読むよ（GETのみ対応）
    char method[8];
    int len;

    if (sscanf(buffer, "%7s %255s", method, request_path) != 2)
    {
        // 400返す
        printf("error: parse method\n");
        len = snprintf(buffer, sizeof(buffer),
                       "HTTP/1.1 400 Bad Request\r\n"
                       "Content-Type: text/html; charset=utf-8\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n");
        write(client_fd, buffer, len);
        return;
    }

    if (strcmp(method, "GET") == 0)
    {
        handle_get(client_fd, request_path);
    }
    else
    {
        // 405返す
        printf("error: method not allowed\n");
        len = snprintf(buffer, sizeof(buffer),
                       "HTTP/1.1 405 Method Not Allowed\r\n"
                       "Content-Type: text/html; charset=utf-8\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n");
        write(client_fd, buffer, len);
        return;
    }
}

void handle_get(int client_fd, const char *request_path)
{
    char file_path[512];
    char buffer[1024];
    build_file_path(request_path, file_path, sizeof(file_path));

    FileData file_data = read_file(file_path);

    int len;
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

    const char *content_type = get_content_type(file_path);

    send_response(client_fd, status, content_type, file_data.data, file_data.size);

    free(file_data.data);
}

void send_response(int client_fd, const char *status, const char *content_type, const void *body, const size_t body_size)
{
    // ヘッダー生成
    char header[512];

    if (content_type == NULL)
    {
        content_type = "text/plain";
    }

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status, content_type, body_size);

    // ボディー生成
    const char *p = body;

    // レスポンス返す
    ssize_t header_written = 0;
    while (header_written < header_len)
    {
        ssize_t written = write(client_fd, header + header_written, header_len - header_written);
        if (written <= 0)
        {
            perror("write failed");
            return;
        }

        header_written += written;
    }

    ssize_t body_written = 0;
    while (body_written < (ssize_t)body_size)
    {
        ssize_t written = write(client_fd, p + body_written, body_size - body_written);
        if (written <= 0)
        {
            perror("write failed");
            return;
        }

        body_written += written;
    }
}

ssize_t get_content_length(const char *header)
{
    const char *line_start = header;
    const char *CONTENT_LENGTH_PREFIX = "Content-Length:";
    const size_t CONTENT_LENGTH_COUNT = strlen(CONTENT_LENGTH_PREFIX);

    while (INFINITY_LOOP)
    {
        const char *line_end = strstr(line_start, LINE_END);

        if (line_end == NULL)
        {
            // 見つからなかった
            break;
        }

        if (line_end == line_start)
        {
            // 空行
            break;
        }

        // リクエストそれぞれ行のコンテンツ長
        const size_t line_length = line_end - line_start;

        if (line_length < CONTENT_LENGTH_COUNT)
        {
            // 次のラインへ
            line_start = line_end + LINE_END_LEN;
            continue;
        }

        // ここに来た段階で、Content-Length:があるか確認

        if (strncasecmp(line_start, CONTENT_LENGTH_PREFIX, CONTENT_LENGTH_COUNT) == 0)
        {
            //
            // 取りあえずポインタ移動
            const char *line_ptr = line_start + CONTENT_LENGTH_COUNT;

            // スペースを飛ばす
            while (line_ptr < line_end && *line_ptr == ' ')
            {
                line_ptr++;
            }

            // この段階でline_ptrは数字の前までポインタは来ている
            return strtol(line_ptr, NULL, 10);
        }

        // 次のラインへ
        line_start = line_end + LINE_END_LEN;
    }

    // なかった
    return -1;
}