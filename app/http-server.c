#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *read_file(const char *ppath);
void parse_request_path(const char *request, char *path_out, size_t size);
const char *get_content_type(const char *path);
void build_file_path(const char *request_path, char *full_path, size_t size);

const int LOOP_INFINITY = 1;

int main(void)
{
    // 待ち受けソケット
    int server_fd;

    // accept後に得られるソケット
    int client_fd;

    // v4アドレスとポートなど
    struct sockaddr_in addr;

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

        // リクエストを読むよ
        char buffer[1024];
        int n = read(client_fd, buffer, sizeof(buffer) - 1);

        if (n > 0)
        {
            buffer[n] = '\0'; // null終端

            printf("REQUEST:\n%s\n", buffer);

            // ファイル読み込み
            char request_path[256];
            parse_request_path(buffer, request_path, sizeof(request_path));

            char file_path[512];
            build_file_path(request_path, file_path, sizeof(file_path));

            char *content = read_file(file_path);

            const char *content_type = get_content_type(file_path);

            if (content == NULL)
            {
                printf("error: read content\n");
                int len = snprintf(buffer, sizeof(buffer),
                                   "HTTP/1.1 404 Not Found\r\n"
                                   "Content-Type: text/html; charset=utf-8\r\n"
                                   "Content-Length: 0\r\n"
                                   "Connection: close\r\n"
                                   "\r\n");
                write(client_fd, buffer, len);
                continue;
            }

            // レスポンスを書く（サイズは動的に）
            size_t content_length = strlen(content);
            size_t response_size = content_length + 512;
            char *response = malloc(response_size);

            if (response == NULL)
            {
                printf("error: allocate response\n");
                free(content);
                close(client_fd);
                continue;
            }

            int len = snprintf(response, response_size,
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "%s",
                               content_type, strlen(content), content);

            write(client_fd, response, len);
            free(content);
            free(response);
        }

        // ソケットを閉じる
        close(client_fd);
    }

    close(server_fd);
}

char *read_file(const char *path)
{
    // ファイルを取得
    FILE *file = fopen(path, "r");

    if (file == NULL)
    {
        return NULL;
    }

    // ファイルサイズを取得
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // メモリ確保
    char *content = malloc(file_size + 1);

    if (content == NULL)
    {
        fclose(file);
        return NULL;
    }

    // ファイルの読み込み
    fread(content, 1, file_size, file);
    content[file_size] = '\0'; // null終端

    fclose(file);
    return content;
}

// リクエストパスの解析
void parse_request_path(const char *request, char *path_out, size_t size)
{
    const char *start = strchr(request, ' ');

    if (start == NULL)
    {
        strncpy(path_out, "/", size);
        return;
    }

    start++;

    const char *end = strchr(start, ' ');

    if (end == NULL)
    {
        strncpy(path_out, "/", size);
        return;
    }

    size_t len = end - start;
    if (len >= size)
        len = size - 1;

    strncpy(path_out, start, len);
    path_out[len] = '\0';
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

    if (strcmp(request_path, "/") == 0)
    {
        snprintf(full_path, size, "%s/http-server.html", base);
    }
    else
    {
        if (strncmp(request_path, "./", 2) == 0)
        {
            snprintf(full_path, size, "%s%s", base, request_path + 1);
        }
        else
        {
            snprintf(full_path, size, "%s%s", base, request_path);
        }

        const char *ext = strrchr(full_path, '.');
        const char *last_slash = strrchr(full_path, '/');

        if (ext == NULL || (last_slash != NULL && ext < last_slash))
        {
            size_t len = strlen(full_path);

            if (len > 0 && full_path[len - 1] != '/')
            {
                strncat(full_path, "/", size - len - 1);
                len++;
            }
            strncat(full_path, "index.html", size - len - 1);
        }
    }
}