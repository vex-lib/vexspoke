// vexspoke/test/net/url_test.c — owner test for net/url.
//
// Proves the URI/auth builders against published vectors and boundary caps:
//   - default ports (https 443, everything else 80, case-insensitive);
//   - RFC 4648 base64 with all three tail paddings, and the exact-capacity
//     refusal (-1) when the output buffer is one byte short;
//   - Basic auth (with a known vector and a null-credential case) and Bearer;
//   - build joins scheme/host/path, omits the default port, keeps a non-default
//     port, prepends a missing slash, and reports truncation as -1.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "net/url.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/** Checks URL-safe base64 encoding against a fixed expected output string. */
static void test_base64(const char *in, const char *want) {
    char out[64];
    int64_t n = Url_base64((const u8*) in, strlen(in), out, sizeof(out));
    CHECK(n == (int64_t) strlen(want));
    CHECK(strcmp(out, want) == 0);
    // Exactly-needed capacity succeeds; one short fails.
    bytes need = 4 * ((strlen(in) + 2) / 3);
    char exact[64];
    CHECK(Url_base64((const u8*) in, strlen(in), exact, need + 1) == (int64_t) need);
    CHECK(Url_base64((const u8*) in, strlen(in), exact, need) == -1);
}

/** Verifies default URL components and their safe initial values. */
static void test_defaults(void) {
    CHECK(Url_defaultPort("https") == 443);
    CHECK(Url_defaultPort("HTTPS") == 443);
    CHECK(Url_defaultPort("http") == 80);
    CHECK(Url_defaultPort("ftp") == 80);
    CHECK(Url_defaultPort(nullptr) == 80);
}

/** Checks credential handling and authorization formatting for URL requests. */
static void test_auth(void) {
    char out[256];
    int64_t n = Url_basicAuth("Aladdin", "open sesame", out, sizeof(out));
    CHECK(n == (int64_t) strlen("Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ=="));
    CHECK(strcmp(out, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==") == 0);

    // nullptr credentials become empty strings: "Basic Og==".
    CHECK(Url_basicAuth(nullptr, nullptr, out, sizeof(out)) == (int64_t) strlen("Basic Og=="));
    CHECK(strcmp(out, "Basic Og==") == 0);

    CHECK(Url_bearerAuth("abc.def", out, sizeof(out)) == (int64_t) strlen("Bearer abc.def"));
    CHECK(strcmp(out, "Bearer abc.def") == 0);
    CHECK(Url_bearerAuth(nullptr, out, sizeof(out)) == 7);
    CHECK(strcmp(out, "Bearer ") == 0);

    // Caps too small are refused.
    CHECK(Url_bearerAuth("token", out, 4) == -1);
    CHECK(Url_basicAuth("a", "b.sh", out, 4) == -1);
}

/** Exercises URL assembly from its configured components and boundary inputs. */
static void test_build(void) {
    char out[256];
    int64_t n = Url_build("https", "example.com", 0, "/path", out, sizeof(out));
    CHECK(n == (int64_t) strlen("https://example.com/path"));
    CHECK(strcmp(out, "https://example.com/path") == 0);

    // https default port 443 is omitted.
    CHECK(Url_build("https", "example.com", 443, "/", out, sizeof(out)) > 0);
    CHECK(strcmp(out, "https://example.com/") == 0);
    // A non-default port is kept.
    CHECK(Url_build("https", "example.com", 8443, "/x", out, sizeof(out)) > 0);
    CHECK(strcmp(out, "https://example.com:8443/x") == 0);
    // http port 80 is the default and omitted; 8080 is kept.
    CHECK(Url_build("http", "h", 80, "/", out, sizeof(out)) > 0);
    CHECK(strcmp(out, "http://h/") == 0);
    CHECK(Url_build("http", "h", 8080, "/", out, sizeof(out)) > 0);
    CHECK(strcmp(out, "http://h:8080/") == 0);
    // A missing leading slash is prepended.
    CHECK(Url_build("http", "h", 0, "seg", out, sizeof(out)) > 0);
    CHECK(strcmp(out, "http://h/seg") == 0);
    // nullptr fallbacks.
    CHECK(Url_build(nullptr, nullptr, 0, nullptr, out, sizeof(out)) > 0);
    CHECK(strcmp(out, "http:///") == 0);

    // Truncation -> -1.
    CHECK(Url_build("https", "example.com", 0, "/path", out, 4) == -1);
    CHECK(Url_build("https", "example.com", 0, "/path", out, 0) == -1);
}

/** Runs the URL owner scenarios and reports aggregate assertion status. */
int main(void) {
    test_defaults();
    test_base64("", "");
    test_base64("f", "Zg==");
    test_base64("fo", "Zm8=");
    test_base64("foo", "Zm9v");
    test_base64("foob", "Zm9vYg==");
    test_base64("fooba", "Zm9vYmE=");
    test_base64("foobar", "Zm9vYmFy");
    test_auth();
    test_build();

    if (g_failures == 0) {
        printf("url_test: all assertions held\n");
        return 0;
    }
    printf("url_test: %d FAILURES\n", g_failures);
    return 1;
}
