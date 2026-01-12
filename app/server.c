#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(void)
{
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[1024];

    // 1. socket作成
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. アドレス設定（ゼロ初期化が重要）
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 3. bind
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));

    // 4. listen
    listen(server_fd, 5);

    printf("listening on port 8080...\n");

    // 5. accept
    client_fd = accept(server_fd, NULL, NULL);
    printf("client connected\n");

    // 6. リクエストを読む（戻り値を確認）
    int n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0)
    {
        perror("read");
        close(client_fd);
        close(server_fd);
        return 1;
    }
    buffer[n] = '\0';
    printf("REQUEST:\n%s\n", buffer);

    // 7. レスポンスを書く（Content-Lengthを正しく）
    const char *body = "Hello world";
    char response[256];

    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "%s",
                       strlen(body), body);

    write(client_fd, response, len);

    // 8. close
    close(client_fd);
    close(server_fd);

    return 0;
}
