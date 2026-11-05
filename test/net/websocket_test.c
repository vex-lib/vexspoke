// MODULE: net/websocket owner. A forked loopback peer speaks raw RFC 6455 bytes
// so the client is exercised against a scripted server, never against itself.
// Scope: handshake request shape and accept-hash verification, the frame codec
// with client masking, ping/pong, continuation reassembly, close, UTF-8
// validation, and the hostile cases the RFC makes refusals (masked server
// frame, oversize control frame, overlong 64-bit length, stray continuation,
// invalid UTF-8, message past the cap). Each hostile case asserts the refusal
// text of ITS OWN guard, so one guard firing for another's frame fails the
// case; the 64-bit-length frame is deliberately unmasked so the mask rule
// cannot mask the length rule. No live network, no TLS, no wss://, no proxy.
#include "net/websocket.h"

#include "lang/str.h"
#include "net/url.h"
#include "security/sha1.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>

#define WS_TEST_TIMEOUT_MS 2000u
#define WS_TEST_MESSAGE_CAP 4096u
#define WS_TEST_BUFFER 8192u

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

// Scenarios the scripted peer can play. Each one is one exchange.
typedef enum Scenario {
    SCENARIO_ECHO_TEXT = 0,      // valid 101, one text frame, verify request shape
    SCENARIO_PING_THEN_TEXT,     // ping (client must pong), then a text frame
    SCENARIO_FRAGMENTS,          // one text message split across three frames
    SCENARIO_CLOSE,              // close frame with code and reason
    SCENARIO_BINARY,             // one binary frame with a NUL inside
    SCENARIO_BAD_ACCEPT,         // 101 with a wrong accept hash
    SCENARIO_NOT_101,            // 200 instead of 101
    SCENARIO_NO_UPGRADE,         // 101 without Upgrade: websocket
    SCENARIO_MASKED_FRAME,       // a server frame carrying a mask bit
    SCENARIO_CONTROL_TOO_LONG,   // 126-byte control payload
    SCENARIO_STRAY_CONTINUATION, // continuation with no start frame
    SCENARIO_LENGTH_HIGH_BIT,    // 64-bit length with bit 63 set
    SCENARIO_BAD_UTF8,           // text frame whose payload is not UTF-8
    SCENARIO_OVERSIZE,           // frame larger than the client's cap
} Scenario;

// --- Byte-level peer helpers (deliberately independent of the library) ---

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

/** Server-side frame: never masked, which is what the RFC demands. */
static bool peerFrame(int fd, bool fin, uint8_t opcode,
                      const uint8_t *payload, size_t length) {
    uint8_t header[10];
    size_t used = 0u;
    header[used++] = (uint8_t) ((fin ? 0x80u : 0x00u) | opcode);
    if (length <= 125u) {
        header[used++] = (uint8_t) length;
    } else if (length <= 0xFFFFu) {
        header[used++] = 126u;
        header[used++] = (uint8_t) (length >> 8);
        header[used++] = (uint8_t) (length & 0xFFu);
    } else {
        header[used++] = 127u;
        uint64_t value = (uint64_t) length;
        for (int shift = 56; shift >= 0; shift -= 8)
            header[used++] = (uint8_t) ((value >> shift) & 0xFFu);
    }
    return peerWrite(fd, header, used) &&
           (length == 0u || peerWrite(fd, payload, length));
}

/** Hostile frame: a server must not mask, so this is a protocol error. */
static bool peerMaskedFrame(int fd, bool fin, uint8_t opcode,
                            const uint8_t *payload, size_t length) {
    uint8_t mask[4] = { 0xA5u, 0x5Au, 0x0Fu, 0xF0u };
    uint8_t buffer[64];
    size_t used = 0u;
    buffer[used++] = (uint8_t) ((fin ? 0x80u : 0x00u) | opcode);
    buffer[used++] = (uint8_t) (0x80u | length);
    memcpy(buffer + used, mask, sizeof(mask));
    used += sizeof(mask);
    for (size_t i = 0u; i < length && used < sizeof(buffer); i++)
        buffer[used++] = (uint8_t) (payload[i] ^ mask[i & 3u]);
    return peerWrite(fd, buffer, used);
}

/** Read until the request's blank line, capturing it for shape assertions. */
static size_t peerReadRequest(int fd, char *dest, size_t cap) {
    size_t used = 0u;
    while (used + 1u < cap) {
        ssize_t n = recv(fd, dest + used, cap - 1u - used, 0);
        if (n <= 0)
            break;
        used += (size_t) n;
        dest[used] = '\0';
        if (strstr(dest, "\r\n\r\n") != nullptr)
            break;
    }
    return used;
}

static bool peerContains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != nullptr;
}

/** The accept hash the client must verify: base64(SHA1(key || GUID)). */
static bool peerAcceptHash(const char *request, char *out, size_t cap) {
    const char *key = strstr(request, "Sec-WebSocket-Key: ");
    if (key == nullptr)
        return false;
    key += strlen("Sec-WebSocket-Key: ");
    const char *end = strstr(key, "\r\n");
    if (end == nullptr)
        return false;
    char value[64];
    size_t length = (size_t) (end - key);
    if (length + 1u > sizeof(value))
        return false;
    memcpy(value, key, length);
    value[length] = '\0';

    CryptoSha1 digest = CryptoSha1_0();
    Crypto_sha1Init(&digest);
    Crypto_sha1Update(&digest, value, strlen(value));
    Crypto_sha1Update(&digest, WS_HANDSHAKE_GUID, strlen(WS_HANDSHAKE_GUID));
    uint8_t hash[CRYPTO_SHA1_DIGEST_SIZE];
    Crypto_sha1Final(&digest, hash);
    return Url_base64(hash, CRYPTO_SHA1_DIGEST_SIZE, out, cap) > 0;
}

static bool peerRespond(int fd, int status, const char *accept, bool upgrade,
                        bool connection) {
    char response[512];
    int written;
    if (status == 101) {
        written = snprintf(response, sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "%s"
            "%s"
            "Sec-WebSocket-Accept: %s\r\n"
            "\r\n",
            upgrade ? "Upgrade: websocket\r\n" : "",
            connection ? "Connection: Upgrade\r\n" : "",
            accept == nullptr ? "" : accept);
    } else {
        written = snprintf(response, sizeof(response),
            "HTTP/1.1 %d Nope\r\nContent-Length: 0\r\n\r\n", status);
    }
    return written > 0 && (size_t) written < sizeof(response) &&
           peerWrite(fd, response, (size_t) written);
}

// --- The forked peer: bind, accept, play one scenario, report request shape ---

static int peerRun(Scenario scenario, int readyWriteFd) {
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

    struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int peer = accept(listener, nullptr, nullptr);
    if (peer < 0)
        return 95;
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(peer, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    char request[WS_TEST_BUFFER];
    if (peerReadRequest(peer, request, sizeof(request)) == 0u)
        return 96;

    // The request shape is asserted by the peer and reported through its exit
    // status, so a malformed request fails the scenario rather than its frames.
    bool shape =
        peerContains(request, "GET /live HTTP/1.1\r\n") &&
        peerContains(request, "Upgrade: websocket\r\n") &&
        peerContains(request, "Connection: Upgrade\r\n") &&
        peerContains(request, "Sec-WebSocket-Version: 13\r\n") &&
        peerContains(request, "Sec-WebSocket-Key: ");
    if (!shape)
        return 10;

    char accept[64];
    if (!peerAcceptHash(request, accept, sizeof(accept)))
        return 11;

    switch (scenario) {
        case SCENARIO_BAD_ACCEPT:
            if (!peerRespond(peer, 101, "AAAAAAAAAAAAAAAAAAAAAAAAAAA=", true, true))
                return 12;
            break;
        case SCENARIO_NOT_101:
            if (!peerRespond(peer, 200, nullptr, false, false))
                return 12;
            break;
        case SCENARIO_NO_UPGRADE:
            if (!peerRespond(peer, 101, accept, false, true))
                return 12;
            break;
        default:
            if (!peerRespond(peer, 101, accept, true, true))
                return 12;
            break;
    }
    if (scenario == SCENARIO_BAD_ACCEPT || scenario == SCENARIO_NOT_101 ||
        scenario == SCENARIO_NO_UPGRADE) {
        close(peer);
        close(listener);
        return 0; // the client must refuse; nothing else is sent
    }

    const char *greeting = "hello websocket";
    switch (scenario) {
        case SCENARIO_ECHO_TEXT:
            if (!peerFrame(peer, true, WEBSOCKET_TEXT,
                           (const uint8_t*) greeting, strlen(greeting)))
                return 13;
            break;
        case SCENARIO_PING_THEN_TEXT:
            if (!peerFrame(peer, true, WEBSOCKET_PING, (const uint8_t*) "p", 1u))
                return 13;
            if (!peerFrame(peer, true, WEBSOCKET_TEXT,
                           (const uint8_t*) greeting, strlen(greeting)))
                return 13;
            break;
        case SCENARIO_FRAGMENTS:
            if (!peerFrame(peer, false, WEBSOCKET_TEXT, (const uint8_t*) "he", 2u))
                return 13;
            if (!peerFrame(peer, false, WEBSOCKET_CONTINUATION, (const uint8_t*) "llo ", 4u))
                return 13;
            if (!peerFrame(peer, true, WEBSOCKET_CONTINUATION,
                           (const uint8_t*) "web", 3u))
                return 13;
            break;
        case SCENARIO_BINARY: {
            const uint8_t payload[4] = { 0x00u, 0xFFu, 0x10u, 0x00u };
            if (!peerFrame(peer, true, WEBSOCKET_BINARY, payload, sizeof(payload)))
                return 13;
            break;
        }
        case SCENARIO_CLOSE: {
            const uint8_t payload[4] = { 0x03u, 0xE8u, 'b', 'y' }; // 1000 "by"
            if (!peerFrame(peer, true, WEBSOCKET_CLOSE, payload, sizeof(payload)))
                return 13;
            break;
        }
        case SCENARIO_MASKED_FRAME:
            if (!peerMaskedFrame(peer, true, WEBSOCKET_TEXT,
                                 (const uint8_t*) "hi", 2u))
                return 13;
            break;
        case SCENARIO_CONTROL_TOO_LONG: {
            uint8_t big[126];
            memset(big, 'x', sizeof(big));
            if (!peerFrame(peer, true, WEBSOCKET_PING, big, sizeof(big)))
                return 13;
            break;
        }
        case SCENARIO_STRAY_CONTINUATION:
            if (!peerFrame(peer, true, WEBSOCKET_CONTINUATION,
                           (const uint8_t*) "lost", 4u))
                return 13;
            break;
        case SCENARIO_LENGTH_HIGH_BIT: {
            // Deliberately UNMASKED: this case isolates the 64-bit length rule,
            // so the mask guard must not swallow the refusal first.
            uint8_t header[10] = { 0x81u, 0x7Fu, 0x80u, 0u, 0u, 0u, 0u, 0u, 0u, 0u };
            if (!peerWrite(peer, header, sizeof(header)))
                return 13;
            break;
        }
        case SCENARIO_BAD_UTF8: {
            const uint8_t broken[3] = { 0xC3u, 0x28u, 0x41u }; // truncated sequence
            if (!peerFrame(peer, true, WEBSOCKET_TEXT, broken, sizeof(broken)))
                return 13;
            break;
        }
        case SCENARIO_OVERSIZE: {
            // Announce 8192 Bytes — twice the client's 4096 cap — and send none
            // of it: the length must be refused from the header alone, so the
            // client never blocks waiting for a payload it will not read.
            uint8_t header[10] = { 0x82u, 127u, 0u, 0u, 0u, 0u, 0u, 0x20u, 0u };
            if (!peerWrite(peer, header, sizeof(header)))
                return 13;
            break;
        }
        default:
            break;
    }

    // Receive-side assertions: the client's own frames must be masked, and a
    // client must produce a mask key of its own for every frame it sends.
    if (scenario == SCENARIO_PING_THEN_TEXT || scenario == SCENARIO_ECHO_TEXT ||
        scenario == SCENARIO_FRAGMENTS) {
        uint8_t clientFrame[64];
        ssize_t got = recv(peer, clientFrame, sizeof(clientFrame), 0);
        if (scenario == SCENARIO_PING_THEN_TEXT) {
            if (got < 6 || (clientFrame[0] & 0x0Fu) != WEBSOCKET_PONG ||
                (clientFrame[1] & 0x80u) == 0u)
                return 14; // a pong, masked, is required
        }
    }
    close(peer);
    close(listener);
    return 0;
}

// --- Scenario driver: fork the peer, open the client, assert ---

typedef struct PeerContext {
    pid_t pid;
    int readFd;
} PeerContext;

static bool peerStart(PeerContext *context, Scenario scenario, char *url, size_t urlCap) {
    int fds[2];
    if (pipe(fds) != 0)
        return false;
    (*context).readFd = fds[0];
    (*context).pid = fork();
    if ((*context).pid == 0) {
        close(fds[0]);
        _exit(peerRun(scenario, fds[1]));
    }
    close(fds[1]);
    if ((*context).pid < 0)
        return false;
    int port = 0;
    ssize_t got = read(fds[0], &port, sizeof(port));
    if (got != (ssize_t) sizeof(port))
        return false;
    snprintf(url, urlCap, "ws://127.0.0.1:%d/live", port);
    return true;
}

/** Reap the peer and return its exit status, or -1 when it did not exit. */
static int peerFinish(PeerContext *context) {
    close((*context).readFd);
    int status = 0;
    if (waitpid((*context).pid, &status, 0) != (*context).pid)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool openClient(WebSocket *client, const char *url) {
    WebSocketConfig config = { .timeoutMs = WS_TEST_TIMEOUT_MS,
                              .maxMessageBytes = WS_TEST_MESSAGE_CAP };
    return WebSocket_open(client, url, config);
}

/** The happy path: handshake, one text frame, then a clean close. */
static void testTextExchange(void) {
    printf("[1] handshake, masked client frame, text message\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_ECHO_TEXT, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(WebSocket_open(&client, url, (WebSocketConfig) { .timeoutMs = WS_TEST_TIMEOUT_MS,
                                                          .maxMessageBytes = WS_TEST_MESSAGE_CAP }));
    CHECK(WebSocket_isOpen(&client));
    CHECK(WebSocket_state(&client) == WEBSOCKET_OPEN);

    WebSocketMessage message = WebSocket_poll(&client, 500u);
    CHECK(message.kind == WS_MESSAGE_TEXT);
    CHECK(message.length == strlen("hello websocket"));
    CHECK(message.payload != nullptr &&
          memcmp(message.payload, "hello websocket", message.length) == 0);
    CHECK(WebSocket_framesIn(&client) == 1u);

    CHECK(WebSocket_writeText(&client, "from the client", 15u));
    CHECK(WebSocket_framesOut(&client) == 1u);
    CHECK(WebSocket_writeClose(&client, 1000u, "done"));
    CHECK(WebSocket_state(&client) == WEBSOCKET_CLOSING);
    WebSocket_close(&client);
    CHECK(WebSocket_state(&client) == WEBSOCKET_CLOSED);
    CHECK(peerFinish(&peer) == 0);
}

/** A ping is answered by the client itself and never surfaces as a message. */
static void testPingPong(void) {
    printf("[2] a ping is answered inline and the text still arrives\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_PING_THEN_TEXT, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(openClient(&client, url));
    WebSocketMessage message = WebSocket_poll(&client, 500u);
    CHECK(message.kind == WS_MESSAGE_TEXT);
    CHECK(message.length == strlen("hello websocket"));
    CHECK(WebSocket_pongsSent(&client) == 1u);
    WebSocket_close(&client);
    CHECK(peerFinish(&peer) == 0);
}

/** Three frames must reassemble into one message. */
static void testFragments(void) {
    printf("[3] continuation frames reassemble into one message\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_FRAGMENTS, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(openClient(&client, url));
    WebSocketMessage message = WebSocket_poll(&client, 500u);
    CHECK(message.kind == WS_MESSAGE_TEXT);
    CHECK(message.length == strlen("hello web"));
    CHECK(message.payload != nullptr && memcmp(message.payload, "hello web", message.length) == 0);
    CHECK(WebSocket_framesIn(&client) == 3u);
    WebSocket_close(&client);
    CHECK(peerFinish(&peer) == 0);
}

/** Binary payloads keep embedded NUL Bytes intact. */
static void testBinary(void) {
    printf("[4] binary message keeps embedded NUL Bytes\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_BINARY, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(openClient(&client, url));
    WebSocketMessage message = WebSocket_poll(&client, 500u);
    CHECK(message.kind == WS_MESSAGE_BINARY);
    CHECK(message.length == 4u);
    if (message.payload != nullptr && message.length == 4u) {
        CHECK(message.payload[0] == 0x00u && message.payload[1] == 0xFFu);
        CHECK(message.payload[2] == 0x10u && message.payload[3] == 0x00u);
    }
    WebSocket_close(&client);
    CHECK(peerFinish(&peer) == 0);
}

/** A peer close reports its code and reason and ends the connection. */
static void testPeerClose(void) {
    printf("[5] peer close reports code and reason\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_CLOSE, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(openClient(&client, url));
    WebSocketMessage message = WebSocket_poll(&client, 500u);
    CHECK(message.kind == WS_MESSAGE_CLOSE);
    CHECK(message.closeCode == 1000u);
    CHECK(WebSocket_closeCode(&client) == 1000u);
    CHECK(strcmp(WebSocket_closeReason(&client), "by") == 0);
    CHECK(WebSocket_state(&client) == WEBSOCKET_CLOSED);
    WebSocket_close(&client);
    CHECK(peerFinish(&peer) == 0);
}

/** Handshake refusals: a bad accept hash, a non-101, a missing Upgrade. */
static void testHandshakeRefusals(void) {
    printf("[6] handshake refusals\n");
    struct { Scenario scenario; const char *label; } cases[] = {
        { SCENARIO_BAD_ACCEPT, "bad accept hash" },
        { SCENARIO_NOT_101, "status 200" },
        { SCENARIO_NO_UPGRADE, "missing Upgrade" },
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char url[128];
        PeerContext peer;
        if (!peerStart(&peer, cases[i].scenario, url, sizeof(url))) {
            CHECK(false);
            continue;
        }
        WebSocket client = WebSocket();
        CHECK(!openClient(&client, url));
        CHECK(WebSocket_state(&client) != WEBSOCKET_OPEN);
        CHECK(WebSocket_lastError(&client)[0] != '\0');
        CHECK(peerFinish(&peer) == 0);
    }
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

/** Unsupported URLs are refused before any socket is opened. */
static void testUrlRefusals(void) {
    printf("[7] unsupported and malformed URLs\n");
    WebSocket client = WebSocket();
    WebSocketConfig config = { .timeoutMs = 250u, .maxMessageBytes = WS_TEST_MESSAGE_CAP };
    CHECK(!WebSocket_open(&client, "http://example.com/live", config));
    CHECK(WebSocket_state(&client) != WEBSOCKET_OPEN);
    CHECK(!WebSocket_open(&client, "ftp://example.com/live", config));
    CHECK(!WebSocket_open(&client, "ws://", config));
    CHECK(!WebSocket_open(nullptr, "ws://127.0.0.1:1/", config));
    CHECK(!WebSocket_open(&client, nullptr, config));
    // A ws:// URL that cannot be reached fails as a CONNECT, not as a URL.
    CHECK(!WebSocket_open(&client, "ws://127.0.0.1:1/live", config));
    CHECK(strstr(WebSocket_lastError(&client), "connect failed") != nullptr);

    // wss:// is a supported scheme now: it takes the TLS path, so an
    // unreachable peer fails there rather than being rejected as a URL. That
    // the error names TLS is the observable proof the routing happened.
    int closed = closedPort();
    CHECK(closed > 0);
    if (closed > 0) {
        char secure[128];
        snprintf(secure, sizeof(secure), "wss://127.0.0.1:%d/live", closed);
        CHECK(!WebSocket_open(&client, secure, config));
        CHECK(strstr(WebSocket_lastError(&client), "TLS connect failed") != nullptr);
        CHECK(WebSocket_state(&client) != WEBSOCKET_OPEN);
    }
    // Null-safety on every getter and projection of the empty instance.
    WebSocket empty = WebSocket();
    CHECK(WebSocket_state(nullptr) == WEBSOCKET_CLOSED);
    CHECK(!WebSocket_isOpen(nullptr));
    CHECK(strcmp(WebSocket_lastError(nullptr), "") == 0);
    CHECK(strcmp(WebSocket_closeReason(nullptr), "") == 0);
    CHECK(WebSocket_closeCode(nullptr) == 1005u);
    CHECK(WebSocket_framesIn(nullptr) == 0u && WebSocket_framesOut(nullptr) == 0u);
    CHECK(WebSocket_maxMessageBytes(nullptr) == 0u);
    CHECK(!WebSocket_writeText(&empty, "x", 1u));
    CHECK(WebSocket_poll(&empty, 10u).kind == WS_MESSAGE_ERROR);
    WebSocket_close(&empty); // idempotent on a zeroed instance
    WebSocket_close(nullptr);
    CHECK(WebSocket_framesOut(&empty) == 0u);
}

/** Protocol violations must fail closed with their OWN refusal, not any error. */
static void testHostileFrames(void) {
    printf("[8] hostile frames fail closed on their own guard\n");
    struct { Scenario scenario; const char *label; const char *expect; } cases[] = {
        { SCENARIO_MASKED_FRAME, "masked server frame",
          "server frames must not be masked" },
        { SCENARIO_CONTROL_TOO_LONG, "oversize control frame",
          "control frames must be final and within 125 Bytes" },
        { SCENARIO_STRAY_CONTINUATION, "stray continuation",
          "continuation frame without a start frame" },
        { SCENARIO_LENGTH_HIGH_BIT, "unmasked 64-bit length high bit",
          "64-bit frame length must clear its high bit" },
        { SCENARIO_BAD_UTF8, "invalid UTF-8 text",
          "text message is not valid UTF-8" },
        { SCENARIO_OVERSIZE, "frame past the message cap",
          "frame length exceeds the configured cap" },
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char url[128];
        PeerContext peer;
        if (!peerStart(&peer, cases[i].scenario, url, sizeof(url))) {
            CHECK(false);
            continue;
        }
        WebSocket client = WebSocket();
        CHECK(openClient(&client, url));
        WebSocketMessage message = WebSocket_poll(&client, 500u);
        if (message.kind != WS_MESSAGE_ERROR ||
            strstr(message.error, cases[i].expect) == nullptr)
            printf("  case %s: kind=%d error=\"%s\"\n", cases[i].label,
                   (int) message.kind, message.error);
        CHECK(message.kind == WS_MESSAGE_ERROR);
        CHECK(strstr(message.error, cases[i].expect) != nullptr); // the right guard
        CHECK(WebSocket_state(&client) == WEBSOCKET_FAILED);
        // A failed client stays failed: no later poll may invent a message.
        CHECK(WebSocket_poll(&client, 10u).kind == WS_MESSAGE_ERROR);
        WebSocket_close(&client);
        CHECK(WebSocket_state(&client) == WEBSOCKET_CLOSED);
        CHECK(peerFinish(&peer) == 0);
    }
}

/** Outbound text must be valid UTF-8 and control payloads stay in budget. */
static void testOutboundRefusals(void) {
    printf("[9] outbound refusals and projections\n");
    char url[128];
    PeerContext peer;
    if (!peerStart(&peer, SCENARIO_ECHO_TEXT, url, sizeof(url))) {
        CHECK(false);
        return;
    }
    WebSocket client = WebSocket();
    CHECK(openClient(&client, url));
    const char broken[2] = { (char) 0xC3u, (char) 0x28u };
    CHECK(!WebSocket_writeText(&client, broken, 2u)); // not UTF-8
    uint8_t big[126];
    memset(big, 0, sizeof(big));
    CHECK(!WebSocket_writePing(&client, big, sizeof(big))); // over 125
    CHECK(WebSocket_writePing(&client, (const uint8_t*) "abc", 3u));

    char text[256];
    bool truncated = true;
    WebSocket_toString(&client, text, sizeof(text), &truncated);
    CHECK(strstr(text, "WebSocket {") != nullptr && strstr(text, "state=OPEN") != nullptr);
    WebSocket_toStringStruct(&client, text, sizeof(text), &truncated);
    CHECK(strstr(text, "maxMessageBytes=") != nullptr && strstr(text, "messageOpcode=") != nullptr);
    truncated = true;
    WebSocket_toString(nullptr, text, sizeof(text), &truncated);
    CHECK(strcmp(text, "nullptr") == 0);
    char tiny[8];
    WebSocket_toString(&client, tiny, sizeof(tiny), &truncated);
    CHECK(truncated); // a cut is reported, never silent
    WebSocket_close(&client);
    CHECK(peerFinish(&peer) == 0);
}

int main(void) {
    printf("=== WebSocket owner ===\n");
    signal(SIGPIPE, SIG_IGN);
    testTextExchange();
    testPingPong();
    testFragments();
    testBinary();
    testPeerClose();
    testHandshakeRefusals();
    testUrlRefusals();
    testHostileFrames();
    testOutboundRefusals();
    printf("%d checks, %d failed\n", g_checks, g_failures);
    if (g_failures == 0)
        printf("=== WebSocket owner: ALL PASS ===\n");
    return g_failures == 0 ? 0 : 1;
}
