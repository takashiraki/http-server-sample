#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

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
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));

    // ここでリッスンするよ（接続町状態にする）
    listen(server_fd, 5);

    printf("listening on port 8080...\n");

    // ここでリクエストを受け入れる
    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0){
        perror("accept failed");
        return 1;
    }

    // リクエストを読むよ
    char buffer[1024];
    int n = read(client_fd, buffer, sizeof(buffer) - 1);

    if (n <= 0)
    {
        perror("read failed");
        close(client_fd);
        close(server_fd);
        return 1;
    }
}