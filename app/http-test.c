#include <stdio.h>
#include <assert.h>
#include <sys/types.h>

// 関数の宣言（ヘッダあればincludeでOK）
ssize_t get_content_length(const char *header);

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

    printf("✅ all tests passed\n");
    return 0;
}
