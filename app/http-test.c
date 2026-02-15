#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <string.h>

// 定数定義
#define OK 0
#define PARSE_ERROR -1

// 構造体定義
typedef struct
{
    char method[8];
    char path[256];
    ssize_t content_length;
} HttpRequest;

// 関数の宣言（ヘッダあればincludeでOK）
ssize_t get_content_length(const char *header);
int parse_http_request(const char *header, HttpRequest *request);

#define CRLF "\r\n"

int main(void)
{
    // =====================
    // 正常系
    // =====================
    assert(get_content_length(
               "POST / HTTP/1.1" CRLF
               "Content-Length: 10" CRLF
                   CRLF) == 10);

    // 小文字（case insensitive）
    assert(get_content_length(
               "content-length: 5" CRLF
                   CRLF) == 5);

    // スペースあり
    assert(get_content_length(
               "Content-Length:    123" CRLF
                   CRLF) == 123);

    // =====================
    // 異常系
    // =====================
    // 無い
    assert(get_content_length(
               "Host: example.com" CRLF
                   CRLF) == -1);

    // 空ヘッダ
    assert(get_content_length(
               CRLF) == -1);

    // 別ヘッダ
    assert(get_content_length(
               "Content-Type: text/plain" CRLF
                   CRLF) == -1);

    // =====================
    // 複数ヘッダ混在
    // =====================
    assert(get_content_length(
               "Host: localhost" CRLF
               "User-Agent: test" CRLF
               "Content-Length: 42" CRLF
                   CRLF) == 42);

    // =====================
    // parse_http_request テスト
    // =====================
    HttpRequest req;

    // 正常系: GET + HTML
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /index.html HTTP/1.1" CRLF
               "Host: localhost" CRLF
                   CRLF,
               &req) == OK);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/index.html") == 0);
    assert(req.content_length == -1);

    // 正常系: GET + CSS
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /style.css HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);
    assert(strcmp(req.method, "GET") == 0);
    assert(strcmp(req.path, "/style.css") == 0);

    // 正常系: GET + JavaScript
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /app.js HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);

    // 正常系: POST + Content-Length
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "POST /api/data HTTP/1.1" CRLF
               "Content-Length: 100" CRLF
                   CRLF,
               &req) == OK);
    assert(strcmp(req.method, "POST") == 0);
    assert(strcmp(req.path, "/api/data") == 0);
    assert(req.content_length == 100);

    // 正常系: 拡張子なし（デフォルト）
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /api HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);

    // 正常系: JSON
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /data.json HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);

    // 正常系: PNG画像
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET /image.png HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);

    // 異常系: 不正なフォーマット（メソッドのみ）
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "GET" CRLF
                   CRLF,
               &req) == PARSE_ERROR);

    // 異常系: 空リクエスト
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               CRLF,
               &req) == PARSE_ERROR);

    // 境界値: メソッド名7文字ぴったり
    memset(&req, 0, sizeof(req));
    assert(parse_http_request(
               "OPTIONS /test HTTP/1.1" CRLF
                   CRLF,
               &req) == OK);
    assert(strcmp(req.method, "OPTIONS") == 0);

    printf("✅ all tests passed\n");
    return 0;
}
