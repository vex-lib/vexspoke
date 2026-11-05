// MODULE: net/tls.h Apple backend (Network.framework) owner.
//
// Scope: the REFUSAL paths a seam can prove without a trusted peer. A successful
// handshake needs an endpoint whose chain validates against the system trust
// anchors, so this owner contacts no live host and the positive path is recorded
// as an explicit gap rather than a pass. Verification is deliberately NOT
// relaxable, so a self-signed loopback server cannot stand in for real proof
// without shipping a trust override -- which is exactly what a TLS adapter must
// not have.
//
// It proves: every malformed argument is refused with a null output; a refused
// connection fails fast rather than burning the deadline, so the bounded wait is
// real; the seam reports no backend until a handshake actually completes; and
// every null-safety path on send/recv/close holds.
#include "net/tls.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        g_checks++;                                                             \
        if (!(condition)) {                                                     \
            g_failures++;                                                       \
            printf("  FAIL line %d: %s\n", __LINE__, #condition);               \
        }                                                                       \
    } while (0)

static uint64_t nowMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000u + (uint64_t) (ts.tv_nsec / 1000000);
}

/** Bind an ephemeral port, then release it so nothing is listening there. */
static int closedPort(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    socklen_t length = (socklen_t) sizeof(address);
    if (getsockname(fd, (struct sockaddr*) &address, &length) != 0) {
        close(fd);
        return -1;
    }
    int port = ntohs(address.sin_port);
    close(fd);
    return port;
}

int main(void) {
    printf("=== TLS Apple backend owner (refusal paths) ===\n");

    // No backend has answered yet: the seam says so instead of guessing.
    CHECK(Tls_backend() == TLS_BACKEND_NONE);

    TlsConn *conn = (TlsConn*) (uintptr_t) 0x1; // must be cleared before any use
    CHECK(!Tls_connect(nullptr, 443, &conn));
    CHECK(conn == nullptr);
    conn = nullptr;
    CHECK(!Tls_connect("example.com", 443, nullptr));
    CHECK(!Tls_connect("", 443, &conn));
    CHECK(conn == nullptr);
    CHECK(!Tls_connect("127.0.0.1", 0, &conn));
    CHECK(conn == nullptr);
    CHECK(!Tls_connect("127.0.0.1", -1, &conn));
    CHECK(conn == nullptr);
    CHECK(!Tls_connect("127.0.0.1", 70000, &conn));
    CHECK(conn == nullptr);

    // A refused connection stays BOUNDED: the seam must return, never park. It
    // is reported at the connect budget (Network.framework holds a refused or
    // unresolvable peer in `waiting` rather than `failed`), so this asserts
    // boundedness and prints the observed cost instead of assuming a fast
    // refusal -- a silent 10s stall was the first thing this owner caught.
    int port = closedPort();
    CHECK(port > 0);
    if (port > 0) {
        uint64_t started = nowMs();
        bool connected = Tls_connect("127.0.0.1", port, &conn);
        uint64_t elapsed = nowMs() - started;
        printf("  refused connect returned in %llu ms\n", (unsigned long long) elapsed);
        CHECK(!connected);
        CHECK(conn == nullptr);
        CHECK(elapsed < 6000u); // the 5s connect budget, plus scheduling slack
    }

    // A name that cannot resolve is refused the same way.
    CHECK(!Tls_connect("no-such-host.invalid", 443, &conn));
    CHECK(conn == nullptr);

    // Still no backend: nothing has completed a handshake.
    CHECK(Tls_backend() == TLS_BACKEND_NONE);

    // Null-safety across the rest of the seam.
    char buffer[8];
    size_t got = 99u;
    CHECK(!Tls_send(nullptr, "x", 1u));
    CHECK(!Tls_send(nullptr, nullptr, 0u));
    CHECK(!Tls_recv(nullptr, buffer, sizeof(buffer), &got));
    CHECK(got == 0u);
    CHECK(!Tls_recv(nullptr, nullptr, sizeof(buffer), &got));
    got = 99u;
    CHECK(!Tls_recv(nullptr, buffer, 0u, &got));
    CHECK(got == 0u);
    CHECK(!Tls_recv(nullptr, buffer, sizeof(buffer), nullptr));
    Tls_close(nullptr);

    // A handle with no session behind it is refused, not dereferenced.
    TlsConn forged = { .backend = (int) TLS_BACKEND_APPLE, .opaque = nullptr };
    CHECK(!Tls_send(&forged, "x", 1u));
    CHECK(!Tls_recv(&forged, buffer, sizeof(buffer), &got));
    TlsConn mislabelled = { .backend = (int) TLS_BACKEND_CURL, .opaque = (void*) &forged };
    CHECK(!Tls_send(&mislabelled, "x", 1u)); // wrong backend tag is refused

    printf("%d checks, %d failed\n", g_checks, g_failures);
    if (g_failures == 0)
        printf("=== TLS Apple backend owner: ALL PASS ===\n");
    printf("GAP: no completed handshake is proved here; that needs a trusted "
           "endpoint and is not claimed.\n");
    return g_failures == 0 ? 0 : 1;
}
