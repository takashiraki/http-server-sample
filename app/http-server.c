#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

const int LOOP_INFINITY = 1;
const int HEADER_END_LEN = 4;
const int HEADER_END_OVERLAP = HEADER_END_LEN - 1;

typedef struct
{
    char *data;
    size_t size;
} FileData;

FileData read_file(const char *path);

const char *get_content_type(const char *path);

void build_file_path(const char *request_path, char *full_path, size_t size);

ssize_t read_http_request(int fd, char *buffer, size_t buffer_size);

void handle_client(int client_fd);

void singchld_handler(int sig);

int main(void)
{
    // 待ち受けソケット
    int server_fd;

    // accept後に得られるソケット
    int client_fd;

    // v4アドレスとポートなど
    struct sockaddr_in addr;

    struct sigaction sa;
    sa.sa_handler = singchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        return 1;
    }

    // AF_INETはIPv4を使うことを示す定数
    // SOCKET_STREAMはTCPを使うことを示す定数
    // 0はプロトコル自動選択
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // アドレス設定（ゼロ初期化）
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;         // IPv4を使うことを示す
    addr.sin_port = htons(8080);       // htonsはOSのエンディアンに合わせる関数
    addr.sin_addr.s_addr = INADDR_ANY; // ここですべてのIPを受け付けるらしい

    // バインドするよ
    // ここで8080ポート向けの全てのIPからのリクエストを受け付けるよう紐づける

    // この1行で「server_fdというソケットを、
    // ポート8080・すべてのIPアドレス（INADDR_ANY）で待ち受けるように設定する」
    // ということをOSに伝えています。
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind failed");
        return 1;
    }

    // ここでリッスンするよ（接続待ち状態にする）
    if (listen(server_fd, 5) < 0)
    {
        perror("listen failed");
        return 1;
    }

    printf("listening on port 8080...\n");

    while (LOOP_INFINITY)
    {
        // ここでリクエストを受け入れる
        client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0)
        {
            perror("accept failed");
            return 1;
        }

        printf("Accepted connection, client_fd=%d\n", client_fd);
        printf("Parenting process ID: %d\n", getpid());

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            close(client_fd);
            continue;
        }

        if (pid == 0)
        {
            printf("Child process ID: %d\n", getpid());
            close(server_fd);

            handle_client(client_fd);

            close(client_fd);
            exit(0);
        }

        // ソケットを閉じる
        close(client_fd);
    }

    close(server_fd);
}

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
    if (strcmp(ext, ".favicon.ico") == 0)
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

ssize_t read_http_request(int fd, char *buffer, size_t buffer_size)
{
    ssize_t total = 0;
    ssize_t n;

    while (total < (ssize_t)(buffer_size - 1))
    {
        n = read(fd, buffer + total, buffer_size - 1 - total);

        if (n < 0)
        {
            perror("read error");
            return -1;
        }

        if (n == 0)
        {
            break;
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
                return total;
            }
        }
    }

    buffer[total] = '\0';
    return total;
}

void handle_client(int client_fd)
{
    char buffer[1024];
    ssize_t request_size = read_http_request(client_fd, buffer, sizeof(buffer));

    if (request_size < 0)
    {
        // 読み込みエラー
        return;
    }

    printf("REQUEST:\n%s\n", buffer);

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
        ;
    }

    if (strcmp(method, "GET") != 0)
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

    char file_path[512];
    build_file_path(request_path, file_path, sizeof(file_path));

    FileData file_data = read_file(file_path);

    if (file_data.data == NULL)
    {
        printf("error: read content\n");
        len = snprintf(buffer, sizeof(buffer),
                       "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: text/html; charset=utf-8\r\n"
                       "Content-Length: 0\r\n"
                       "Connection: close\r\n"
                       "\r\n");
        write(client_fd, buffer, len);
        return;
    }

    const char *content_type = get_content_type(file_path);

    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              content_type, file_data.size);

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
    while (body_written < (ssize_t)(file_data.size))
    {
        ssize_t written = write(client_fd, file_data.data + body_written, file_data.size - body_written);
        if (written <= 0)
        {
            perror("write failed");
        }

        body_written += written;
    }

    free(file_data.data);
}

void singchld_handler(int sig)
{
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}