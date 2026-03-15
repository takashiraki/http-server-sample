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

#include "http.h"
#include "file.h"

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

#define HEADER_END "\r\n\r\n"

#define READ_PIPE 0
#define WRITE_PIPE 1

#define WAIT_PID_NO_OPTIONS 0

#define BYTE_UNIT_SIZE 1

typedef struct
{
    char method[8];
    char path[256];
    ssize_t content_length;
} HttpRequest;

const char *get_content_type(const char *path);
void build_file_path(const char *request_path, char *full_path, size_t size);
ssize_t read_http_header(int fd, char *buffer, size_t buffer_size, ssize_t *header_length);
int parse_http_request(const char *header, HttpRequest *request);
void handle_client(int client_fd);
void singchld_handler(int sig);
void handle_get(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type);
ssize_t get_content_length(const char *header);
void handle_post(int client_fd, HttpRequest req, char *buffer, ssize_t header_size, ssize_t request_size, const char *content_type, const char *request_file_type);
void handle_php(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type);
void handle_get_static(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type);
void send_response(int client_fd, const char *status, const char *content_type, const void *body, const size_t body_size);

/**
 * 責務：HTTP関連
 */
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

/**
 * 責務：HTTP関連
 */
const char *get_request_file_type(const char *path)
{
    const char *ext = strrchr(path, '.');

    if (ext == NULL)
        return "text/html";

    if (strcmp(ext, ".php") == 0)
        return "php";

    return "static";
}

/**
 * 責務：HTTP関連
 */
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

/**
 * 責務：HTTP関連
 */
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

/**
 * 責務：HTTP関連
 */
int parse_http_request(const char *header, HttpRequest *request)
{
    char method[8];
    char type[16];
    char path[256];

    if (sscanf(header, "%7s %255s", method, path) != 2)
        return PARSE_ERROR;

    snprintf(request->method, sizeof(request->method), "%s", method);
    snprintf(request->path, sizeof(request->path), "%s", path);

    request->content_length = get_content_length(header);

    return OK;
}

/**
 * 責務：HTTP関連
 */
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
    const char *request_file_type = get_request_file_type(file_path);

    if (strcmp(req.method, "GET") == 0)
    {
        handle_get(client_fd, req.path, content_type, file_path, request_file_type);
    }
    else if (strcmp(req.method, "POST") == 0)
    {
        handle_post(client_fd, req, buffer, header_size, request_size, content_type, request_file_type);
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

/**
 * 責務：HTTP関連
 */
void handle_get(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type)
{
    if (strcmp(request_file_type, "static") == 0)
    {
        handle_get_static(client_fd, request_path, content_type, file_path, request_file_type);
        return;
    }
    else if (strcmp(request_file_type, "php") == 0)
    {
        handle_php(client_fd, request_path, content_type, file_path, request_file_type);
        return;
    }
}

/**
 * 責務：HTTP関連
 */
void handle_get_static(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type)
{
    const char *status;
    FileData file_data;

    file_data = read_file(file_path);

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

/**
 * Handle PHP files
 * 責務：HTTP関連
 * fork → execute php-cgi → read output from pipe → send response
 * TODO: 責務分割したい
 * - パイプ作成 + fork + 孫プロセス側のexec実行
 * - パイプからのstdoutとstderrの読み取り
 * - CGIレスポンスパース
 * - ログ書き込み
 */
void handle_php(int client_fd, const char *request_path, const char *content_type, const char *file_path, const char *request_file_type)
{

    int pipefd[2], errpipefd[2];

    // パイプの作成コケたら普通のエラー
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        return;
    }

    if (pipe(errpipefd) == -1)
    {
        perror("pipe");

        // パイプは閉じておこう
        close(pipefd[READ_PIPE]);
        close(pipefd[WRITE_PIPE]);
        return;
    }

    pid_t gpid = fork();
    if (gpid == 0)
    {
        // 孫プロセス側での処理
        printf("Executing PHP CGI for %s\n", file_path);

        // この段階では親プロセスと子プロセス両方で
        // パイプの両端が開いている状態なので、必要のない端は閉じる
        close(pipefd[READ_PIPE]);

        // PHP-CGIが書き込み側なので、標準出力パイプに複写
        dup2(pipefd[WRITE_PIPE], STDOUT_FILENO);
        close(pipefd[WRITE_PIPE]);

        close(errpipefd[READ_PIPE]);
        dup2(errpipefd[WRITE_PIPE], STDERR_FILENO);
        close(errpipefd[WRITE_PIPE]);

        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("REQUEST_METHOD", "GET", 1);
        setenv("SCRIPT_FILENAME", file_path, 1);
        setenv("REDIRECT_STATUS", "200", 1);

        execlp("php-cgi", "php-cgi", NULL);
        perror("execlp");
        exit(1);
    }
    else if (gpid > 0)
    {
        // 　子プロセス側での処理
        printf("Forked process for PHP CGI, PID=%d\n", gpid);

        // 子プロセスは書き込みしないので、すぐに書き込みパイプ閉じちゃう
        close(pipefd[WRITE_PIPE]);
        close(errpipefd[WRITE_PIPE]);

        // 256kb
        char cgi_buf[256 * 1024];
        ssize_t n;
        size_t total = 0;
        char *php_output = NULL;

        while ((n = read(pipefd[READ_PIPE], cgi_buf, sizeof(cgi_buf))) > 0)
        {
            php_output = realloc(php_output, total + n);

            if (php_output == NULL)
            {
                perror("realloc");
                close(pipefd[READ_PIPE]);
                close(errpipefd[READ_PIPE]);
                return;
            }

            memcpy(php_output + total, cgi_buf, n);
            total += n;
        }

        char cgi_err_buf[256 * 1024];
        ssize_t err_n;
        size_t err_total = 0;
        char *cgi_error = NULL;

        while ((err_n = read(errpipefd[READ_PIPE], cgi_err_buf, sizeof(cgi_err_buf))) > 0)
        {
            // メモリ確保し直し
            // （一個前のループまでで読み込んだ量と今回読み込んだ量の合計サイズにする）
            cgi_error = realloc(cgi_error, err_total + err_n);

            if (cgi_error == NULL)
            {
                perror("realloc");
                close(pipefd[READ_PIPE]);
                close(errpipefd[READ_PIPE]);
                free(php_output);
                return;
            }

            // エラー内容を蓄積
            memcpy(cgi_error + err_total, cgi_err_buf, err_n);

            // 総エラーサイズを更新
            err_total += err_n;
        }

        close(pipefd[READ_PIPE]);
        close(errpipefd[READ_PIPE]);

        // exit code取得
        int wstatus = 0;
        waitpid(gpid, &wstatus, WAIT_PID_NO_OPTIONS);

        FILE *debug_log_file = fopen("/tmp/php_cgi_debug.log", "a");
        FILE *error_log_file = fopen("/tmp/php_cgi_error.log", "a");

        char *body = NULL;

        if (php_output)
        {
            body = strstr(php_output, "\r\n\r\n");
        }

        size_t body_offset = body ? (body - php_output) + 4 : 0;

        char *status_line = NULL;

        if (php_output)
        {
            status_line = strstr(php_output, "Status: ");
        }

        if (status_line)
        {
            char *status_end = strstr(status_line, LINE_END);

            if (status_end)
            {
                *status_end = '\0';
                status_line += strlen("Status: ");

                printf("Extracted Status Line: %s\n", status_line);
            }
        }

        if (cgi_error && err_total > 0)
        {
            fwrite("PHP CGI Error Output:\n", 1, strlen("PHP CGI Error Output:\n"), error_log_file);
            fwrite(cgi_error, BYTE_UNIT_SIZE, err_total, error_log_file);
            fputc('\n', error_log_file);
            fclose(error_log_file);
            fclose(debug_log_file);

            send_response(client_fd,
                          "500 Internal Server Error",
                          "text/plain",
                          cgi_error,
                          err_total);

            free(cgi_error);
            free(php_output);

            return;
        }

        printf("PHP CGI output received, total size=%zu\n", total);

        if (debug_log_file)
        {
            fwrite("PHP CGI Debug Log\n", 1, strlen("PHP CGI Debug Log\n"), debug_log_file);
            fprintf(debug_log_file, "Response content size: %zu bytes\n", total);
            fputc('\n', debug_log_file);
            fputs("-------------- values from pipe --------------\n", debug_log_file);
            fwrite(cgi_error, BYTE_UNIT_SIZE, err_total, debug_log_file);
            fputc('\n', debug_log_file);
            fputs("-------------- exit status --------------\n", debug_log_file);
            fprintf(debug_log_file, "Exit code: %d\n", WEXITSTATUS(wstatus));

            // 調査ログ
            if (strstr(status_line ? status_line : php_output, "500 Internal Server Error") != NULL)
            {
                fprintf(stderr, "PHP CGI indicated an internal server error.\n");
            }

            fclose(debug_log_file);
        }

        send_response(client_fd,
                      status_line ? status_line : "200 OK",
                      "text/html",
                      php_output + body_offset,
                      total - body_offset);

        free(php_output);
        return;
    }
    else
    {
        perror("fork");
        return;
    }
}

/**
 * 責務：HTTP関連
 */
void handle_post(int client_fd, HttpRequest req, char *buffer, ssize_t header_size, ssize_t request_size, const char *content_type, const char *request_file_type)
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
    // ヒープバッファオーバーフロー対策
    if (already_read_body_size > req.content_length)
    {
        already_read_body_size = req.content_length;
    }

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

/**
 * 責務：HTTP関連
 */
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

/**
 * 責務：HTTP関連
 */
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
