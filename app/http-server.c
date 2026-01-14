#include <arpa/inet.h>
#include <string.h>

int main(void)
{
    // server_fd → 待ち受けソケット
    // client_fd → accept後に得られるソケット
    int server_fd, client_fd;

    // v4アドレスとポート準備
    struct sockaddr_in addr;

    // AF_INETはIPv4を使うことを示す定数
    // SOCKET_STREAMはTCPを使うことを示す定数
    // 0はプロトコル自動選択
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // アドレス設定（ゼロ初期化）
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; // IPv4を使うことを示す
    addr.sin_port = htons(8080); // htonsはOSのエンディアンに合わせる関数
    addr.sin_addr.s_addr = INADDR_ANY; // ここですべてのIPを受け付けるらしい
}