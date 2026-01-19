#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *read_file(const char *ppath);

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

    while (1)
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
            char *content = read_file("/var/www/html/http-server.html");

            if (content == NULL)
            {
                printf("error: read content\n");
                close(client_fd);
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
                               "Content-Type: text/html\r\n"
                               "Content-Length: %zu\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "%s",
                               strlen(content), content);

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