// MODULE: net/websocket smoke + idle cost.
//
// Two questions this answers, in two tiers:
//
//  1. ALWAYS (offline): does an idle session cost CPU? A forked loopback peer
//     completes a handshake, then goes quiet while the client polls for several
//     seconds. The client never spins -- WebSocket_poll sleeps inside poll() in
//     bounded slices -- so the process's own user+sys time must stay near zero
//     across the whole idle window, and the client must still wake on the one
//     message the peer sends late. Both are asserted, not asserted-at.
//
//  2. OPT-IN (live): does the client agree with a real server? Set
//     WS_SMOKE_URL=wss://host/path and the client round-trips one text frame
//     against that endpoint, which is the only place a completed TLS handshake
//     and real-server framing are ever proved. The shared suite never contacts
//     it (the Offline-Proof Law); an unset URL is an explicit skip.
#include "net/websocket.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

#include "net/url.h"
#include "security/sha1.h"

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

/** The calling process's own CPU time, user + system, in milliseconds. */
static uint64_t selfCpuMs(void) {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        return 0u;
    uint64_t user = (uint64_t) usage.ru_utime.tv_sec * 1000u +
                    (uint64_t) (usage.ru_utime.tv_usec / 1000);
    uint64_t system = (uint64_t) usage.ru_stime.tv_sec * 1000u +
                      (uint64_t) (usage.ru_stime.tv_usec / 1000);
    return user + system;
}

static bool peerWrite(int fd, const void *bytes, size_t length) {
    const uint8_t *p = (const uint8_t*) bytes;
    size_t sent = 0u;
    while (sent < length) {
        ssize_t n = send(fd, p + sent, length - sent, 0);
        if (n <= 0)
            return false;
        sent += (size_t) n;
    }
    return true;
}

/** The peer: handshake, wait out the idle window, then send one late message. */
static int peerRun(int readyWriteFd, uint32_t quietMillis) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return 90;
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(listener, (struct sockaddr*) &address, sizeof(address)) != 0)
        return 91;
    socklen_t length = (socklen_t) sizeof(address);
    if (getsockname(listener, (struct sockaddr*) &address, &length) != 0)
        return 92;
    if (listen(listener, 1) != 0)
        return 93;
    int port = ntohs(address.sin_port);
    if (write(readyWriteFd, &port, sizeof(port)) != (ssize_t) sizeof(port))
        return 94;
    close(readyWriteFd);

    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int peer = accept(listener, nullptr, nullptr);
    if (peer < 0)
        return 95;
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char request[4096];
    size_t used = 0u;
    while (used + 1u < sizeof(request)) {
        ssize_t n = recv(peer, request + used, 1u, 0);
        if (n <= 0)
            break;
        used += (size_t) n;
        request[used] = '\0';
        if (used >= 4u && strstr(request, "\r\n\r\n") != nullptr)
            break;
    }
    const char *key = strstr(request, "Sec-WebSocket-Key: ");
    if (key == nullptr)
        return 96;
    key += strlen("Sec-WebSocket-Key: ");
    const char *keyEnd = strstr(key, "\r\n");
    if (keyEnd == nullptr)
        return 97;
    char value[64];
    size_t keyLength = (size_t) (keyEnd - key);
    if (keyLength + 1u > sizeof(value))
        return 98;
    memcpy(value, key, keyLength);
    value[keyLength] = '\0';
    CryptoSha1 digest = CryptoSha1_0();
    Crypto_sha1Init(&digest);
    Crypto_sha1Update(&digest, value, strlen(value));
    Crypto_sha1Update(&digest, WS_HANDSHAKE_GUID, strlen(WS_HANDSHAKE_GUID));
    uint8_t hash[CRYPTO_SHA1_DIGEST_SIZE];
    Crypto_sha1Final(&digest, hash);
    char accept[64];
    if (Url_base64(hash, CRYPTO_SHA1_DIGEST_SIZE, accept, sizeof(accept)) <= 0)
        return 99;
    char response[256];
    int written = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
    if (written <= 0 || (size_t) written >= sizeof(response) ||
        !peerWrite(peer, response, (size_t) written))
        return 100;

    // Go quiet for the whole window the parent is measuring, then say one word.
    struct timespec quiet = { .tv_sec = (time_t) (quietMillis / 1000u),
                              .tv_nsec = (long) ((quietMillis % 1000u) * 1000000L) };
    nanosleep(&quiet, nullptr);
    const char *late = "late";
    uint8_t header[4] = { 0x81u, 0x04u, 0u, 0u };
    if (!peerWrite(peer, header, 2u) || !peerWrite(peer, late, 4u))
        return 101;
    // Give the client a moment to read it, then hang up.
    struct timespec tail = { .tv_sec = 1, .tv_nsec = 0 };
    nanosleep(&tail, nullptr);
    close(peer);
    close(listener);
    return 0;
}

/** An idle session must cost ~no CPU and must still wake on a late message. */
static void testIdleCost(void) {
    printf("[1] idle cost: a quiet session must not burn CPU\n");
    const uint32_t quietMillis = 3000u;
    int fds[2];
    if (pipe(fds) != 0) {
        CHECK(false);
        return;
    }
    pid_t child = fork();
    if (child == 0) {
        close(fds[0]);
        _exit(peerRun(fds[1], quietMillis));
    }
    close(fds[1]);
    int port = 0;
    if (read(fds[0], &port, sizeof(port)) != (ssize_t) sizeof(port)) {
        CHECK(false);
        return;
    }
    close(fds[0]);

    char url[128];
    snprintf(url, sizeof(url), "ws://127.0.0.1:%d/live", port);
    WebSocket client = WebSocket();
    WebSocketConfig config = { .timeoutMs = 2000u, .maxMessageBytes = 4096u };
    CHECK(WebSocket_open(&client, url, config));
    CHECK(WebSocket_isOpen(&client));

    // Poll for the whole quiet window. Every call sleeps in the kernel until its
    // budget expires; none of them spins.
    uint64_t started = nowMs();
    uint64_t cpuBefore = selfCpuMs();
    uint32_t polls = 0u;
    WebSocketMessage message = WebSocket_poll(&client, 100u);
    while (message.kind == WS_MESSAGE_NONE && nowMs() - started < quietMillis + 1500u) {
        polls++;
        message = WebSocket_poll(&client, 100u);
    }
    uint64_t cpuSpent = selfCpuMs() - cpuBefore;
    uint64_t elapsed = nowMs() - started;

    printf("  idled %llu ms over %u polls, self CPU %llu ms\n",
           (unsigned long long) elapsed, polls, (unsigned long long) cpuSpent);
    // A spin loop would spend ~elapsed of CPU. Sleeping in poll() costs a
    // fraction of that; 10% of the window is a generous ceiling that still
    // fails loudly if anyone turns the wait into a busy loop.
    CHECK(cpuSpent < elapsed / 10u + 20u);
    CHECK(polls >= 20u); // it really waited, rather than returning early
    CHECK(message.kind == WS_MESSAGE_TEXT); // and still woke on the late word
    CHECK(message.length == 4u && memcmp(message.payload, "late", 4u) == 0);
    CHECK(message.payload != nullptr);

    WebSocket_close(&client);
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/**
 * Opt-in live round-trip. The shared suite never contacts the network; this runs
 * only when a URL is supplied, and it is the only place a completed TLS
 * handshake and a foreign server's framing are proved.
 */
static void testLiveRoundTrip(void) {
    const char *url = getenv("WS_SMOKE_URL");
    if (url == nullptr || url[0] == '\0') {
        printf("[2] live round-trip: SKIP (set WS_SMOKE_URL to run it)\n");
        return;
    }
    printf("[2] live round-trip against %s\n", url);
    WebSocket client = WebSocket();
    WebSocketConfig config = { .timeoutMs = 10000u, .maxMessageBytes = 1u << 20 };
    if (!WebSocket_open(&client, url, config)) {
        printf("  open failed: %s\n", WebSocket_lastError(&client));
        g_failures++;
        g_checks++;
        return;
    }
    printf("  handshake accepted\n");
    const char *probe = "vexgraph-websocket-smoke";
    CHECK(WebSocket_writeText(&client, probe, strlen(probe)));
    // A second frame on the SAME connection: the first send must not have shut
    // the write side, or every later send silently fails.
    const char *second = "second-frame";
    CHECK(WebSocket_writeText(&client, second, strlen(second)));
    // Not every live peer echoes: an echo server returns the probe, a real
    // gateway sends its own first frame (Discord's HELLO arrives unprompted and
    // needs no credentials). So this asserts that a TEXT frame arrives at all,
    // and reports which kind of peer answered.
    uint64_t started = nowMs();
    WebSocketMessage message = WebSocket_poll(&client, 5000u);
    while (message.kind == WS_MESSAGE_NONE && nowMs() - started < 8000u)
        message = WebSocket_poll(&client, 500u);
    if (message.kind == WS_MESSAGE_TEXT && message.payload != nullptr) {
        size_t shown = message.length < 120u ? message.length : 120u;
        bool echoed = message.length == strlen(probe) &&
                      memcmp(message.payload, probe, message.length) == 0;
        printf("  first frame: %llu Bytes%s\n", (unsigned long long) message.length,
               echoed ? " (echoed our probe)" : "");
        printf("  payload: %.*s\n", (int) shown, (const char*) message.payload);
        CHECK(message.length > 0u);
    } else {
        printf("  no frame: kind=%d error=%s\n", (int) message.kind, message.error);
        CHECK(message.kind == WS_MESSAGE_TEXT);
    }
    WebSocket_close(&client);
}

/**
 * Opt-in live tier against Discord's gateway, which answers BEFORE it
 * authenticates: HELLO arrives unprompted, a heartbeat is acked, and an
 * intentionally invalid IDENTIFY is refused. No token is used, stored or needed,
 * so this proves the SEND path, the heartbeat bookkeeping and a real close
 * against a production server. JSON is only searched, never parsed — reading
 * Discord's payloads is the gateway client's job, not the transport's.
 */
static void testDiscordGateway(void) {
    if (getenv("WS_SMOKE_DISCORD") == nullptr) {
        printf("[3] discord gateway: SKIP (set WS_SMOKE_DISCORD=1 to run it)\n");
        return;
    }
    printf("[3] discord gateway, no token\n");
    WebSocket client = WebSocket();
    WebSocketConfig config = { .timeoutMs = 10000u, .maxMessageBytes = 1u << 20 };
    if (!WebSocket_open(&client, "wss://gateway.discord.gg/?v=10&encoding=json", config)) {
        printf("  open failed: %s\n", WebSocket_lastError(&client));
        g_failures++;
        g_checks++;
        return;
    }
    printf("  handshake accepted\n");

    WebSocketMessage hello = WebSocket_poll(&client, 5000u);
    CHECK(hello.kind == WS_MESSAGE_TEXT);
    bool isHello = hello.kind == WS_MESSAGE_TEXT &&
        memmem(hello.payload, hello.length, "\"op\":10", 7u) != nullptr;
    bool hasInterval = hello.kind == WS_MESSAGE_TEXT &&
        memmem(hello.payload, hello.length, "\"heartbeat_interval\"", 20u) != nullptr;
    printf("  HELLO: %llu Bytes, op10=%s, heartbeat_interval=%s\n",
           (unsigned long long) hello.length, isHello ? "yes" : "NO",
           hasInterval ? "present" : "MISSING");
    CHECK(isHello);
    CHECK(hasInterval);

    // A heartbeat before identification must be acknowledged. Discord's docs:
    // heartbeats come first, IDENTIFY after, and `d` is null until a sequence
    // number has been seen.
    const char *beat = "{\"op\":1,\"d\":null}";
    CHECK(WebSocket_writeText(&client, beat, strlen(beat)));
    WebSocketMessage ack = WebSocket_poll(&client, 5000u);
    bool acked = ack.kind == WS_MESSAGE_TEXT &&
        memmem(ack.payload, ack.length, "\"op\":11", 7u) != nullptr;
    printf("  heartbeat: kind=%d%s\n", (int) ack.kind, acked ? ", op 11 ACK" : "");
    CHECK(acked);

    // A deliberately invalid IDENTIFY must be refused, loudly.
    const char *identify =
        "{\"op\":2,\"d\":{\"token\":\"vexgraph-smoke-invalid\",\"intents\":0,"
        "\"properties\":{\"os\":\"macos\",\"browser\":\"vexgraph\","
        "\"device\":\"vexgraph\"}}}";
    CHECK(WebSocket_writeText(&client, identify, strlen(identify)));
    WebSocketMessage refused = WebSocket_poll(&client, 8000u);
    bool invalidSession = refused.kind == WS_MESSAGE_TEXT &&
        memmem(refused.payload, refused.length, "\"op\":9", 6u) != nullptr;
    bool closed = refused.kind == WS_MESSAGE_CLOSE;
    printf("  invalid IDENTIFY: kind=%d closeCode=%u%s\n", (int) refused.kind,
           (unsigned) refused.closeCode, invalidSession ? " (op 9 invalid session)" : "");
    CHECK(closed || invalidSession);
    if (closed)
        printf("  refused with close code %u: \"%s\"\n",
               (unsigned) WebSocket_closeCode(&client), WebSocket_closeReason(&client));

    // The exchange really moved frames: HELLO and the ACK at minimum.
    CHECK(WebSocket_framesIn(&client) >= 2u);
    WebSocket_close(&client);
}

int main(void) {
    printf("=== WebSocket smoke: idle cost, live round-trip, discord gateway ===\n");
    signal(SIGPIPE, SIG_IGN);
    testIdleCost();
    testLiveRoundTrip();
    testDiscordGateway();
    printf("%d checks, %d failed\n", g_checks, g_failures);
    if (g_failures == 0)
        printf("=== WebSocket smoke: ALL PASS ===\n");
    return g_failures == 0 ? 0 : 1;
}
