#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "http.h"

const int LOOP_INFINITY = 1;

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
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    int yes = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
    {
        perror("setsockopt failed");
        return 1;
    }

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

void singchld_handler(int sig)
{
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}