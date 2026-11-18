// net/http.c — HTTP/1.1 client & micro server over BSD sockets
// (Legacy: net/HTTPClient.java / HTTPServer.java / PollRequest.java port).
//
// The legacy drove libcurl through FFM; here the transport is ours: one
// blocking TCP connection per request, header block parsed in place, body
// received per Content-Length or chunked framing into the caller's buffer.
// Zero allocation — every buffer is caller-owned.


#if !defined(_GNU_SOURCE) && !defined(VEX_PLATFORM_APPLE)
#define _GNU_SOURCE // strcasestr
#endif

#include "vex.h"
#include "net/http.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "net/url.h"
#include "net/tls.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Http
 * ============================================================================
 * HTTP/1.1 client and micro server over BSD sockets: one blocking TCP
 * connection per request, header block parsed in place, body received per
 * Content-Length or chunked framing into the caller's buffer — zero
 * allocation, every buffer caller-owned. The client fails closed on CRLF
 * injection and refuses https without a TLS backend rather than downgrading.
 * The micro server runs a single accept thread that polls with a 100ms
 * budget (the Bounded Wait Law) so stop stays cooperative, and rejects
 * attacker Content-Lengths that overflow the fixed server body box.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Http (net/http.c)
 * ============================================================================
 * HTTP/1.1 client & micro server over BSD sockets
 *
 * STRUCT FIELDS (Mirroring net/http.h):
 * ----------------------------------------------------------------------------
 *   HttpHeader {
 *     const char *name; // field/entry name
 *     const char *value; // payload value
 *   }
 *   HttpRequest {
 *     const char *scheme; // "http" default, "https" requires TLS backend
 *     const char *method; // "GET", "POST", ... default "GET"
 *     const char *host; // required
 *     i32 port; // 0 => 80
 *     const char *path; // default "/"
 *     const HttpHeader *headers; // header array (headerCount entries)
 *     u32 headerCount; // header array length
 *     const char *body; // nullptr = no body
 *     bytes bodyLen; // message body state
 *     u32 timeoutMs; // connect+read budget, default 5000
 *   }
 *   HttpResponse {
 *     bool ok; // transport-level success; status may still be 4xx
 *     i32 status; // HTTP status code
 *     char contentType[128]; // response MIME type
 *     char *body; // caller-owned receive buffer
 *     bytes bodyCap; // receive buffer capacity
 *     bytes bodyLen; // received body length
 *   }
 *   HttpExchange {
 *     char method[16]; // HTTP method
 *     char path[1024]; // request path
 *     const char *body; // request body view
 *     bytes bodyLen; // request body length
 *   }
 *   HttpServer {
 *     i32 listenFd; // listen socket fd
 *     pthread_t thread; // background thread handle
 *     volatile bool running; // run-state flag
 *     HttpHandler handler; // per-connection request handler
 *     any userdata; // opaque callback context
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Http_perform(req, resp)
 *   - HttpServer_start(server, port, handler, userdata, portOut)
 *   - HttpServer_stop(server)
 *   - Http_respond(clientFd, status, contentType, body, bodyLen)
 * ============================================================================
 */



;;GETTER
static inline const char * HttpRequest_body(const HttpRequest *s) { return (*s).body; }
;;GETTER
static inline bytes HttpRequest_bodyLen(const HttpRequest *s) { return (*s).bodyLen; }
;;GETTER
static inline u32 HttpRequest_headerCount(const HttpRequest *s) { return (*s).headerCount; }
;;GETTER
static inline const HttpHeader * HttpRequest_headers(const HttpRequest *s) { return (*s).headers; }
;;GETTER
static inline const char * HttpRequest_host(const HttpRequest *s) { return (*s).host; }
;;GETTER
static inline const char * HttpRequest_method(const HttpRequest *s) { return (*s).method; }
;;GETTER
static inline const char * HttpRequest_path(const HttpRequest *s) { return (*s).path; }
;;GETTER
static inline i32 HttpRequest_port(const HttpRequest *s) { return (*s).port; }
;;GETTER
static inline const char * HttpRequest_scheme(const HttpRequest *s) { return (*s).scheme; }
;;GETTER
static inline u32 HttpRequest_timeoutMs(const HttpRequest *s) { return (*s).timeoutMs; }

;;GETTER
static inline char * HttpResponse_body(const HttpResponse *s) { return (*s).body; }
;;SETTER
static inline fn HttpResponse_setBody(HttpResponse *s, char * v) { (*s).body = v; }
;;GETTER
static inline bytes HttpResponse_bodyCap(const HttpResponse *s) { return (*s).bodyCap; }
;;SETTER
static inline fn HttpResponse_setBodyCap(HttpResponse *s, bytes v) { (*s).bodyCap = v; }
;;SETTER
static inline fn HttpResponse_setBodyLen(HttpResponse *s, bytes v) { (*s).bodyLen = v; }
;;GETTER
static inline char *HttpResponse_contentTypeMut(HttpResponse *s) { return (*s).contentType; }
;;SETTER
static inline fn HttpResponse_setOk(HttpResponse *s, bool v) { (*s).ok = v; }
;;SETTER
static inline fn HttpResponse_setStatus(HttpResponse *s, i32 v) { (*s).status = v; }

;;GETTER
static inline HttpHandler HttpServer_handler(const HttpServer *s) { return (*s).handler; }
;;SETTER
static inline fn HttpServer_setHandler(HttpServer *s, HttpHandler v) { (*s).handler = v; }
;;GETTER
static inline i32 HttpServer_listenFd(const HttpServer *s) { return (*s).listenFd; }
;;SETTER
static inline fn HttpServer_setListenFd(HttpServer *s, i32 v) { (*s).listenFd = v; }
;;GETTER
static inline bool HttpServer_running(const HttpServer *s) { return (*s).running; }
;;SETTER
static inline fn HttpServer_setRunning(HttpServer *s, volatile bool v) { (*s).running = v; }
;;GETTER
static inline pthread_t HttpServer_thread(const HttpServer *s) { return (*s).thread; }
;;GETTER
static inline pthread_t *HttpServer_threadMut(HttpServer *s) { return &(*s).thread; }
;;GETTER
static inline any HttpServer_userdata(const HttpServer *s) { return (*s).userdata; }
;;SETTER
static inline fn HttpServer_setUserdata(HttpServer *s, any v) { (*s).userdata = v; }

#define HEADER_BLOCK_CAP 8192
#define RECV_TIMEOUT_DEFAULT_MS 5000
#define SERVER_BODY_CAP 16384

// Handlers run inline on the single accept thread, so one shared receive
// buffer is safe.
static char s_serverBody[SERVER_BODY_CAP];

static fn applyTimeout(i32 fd, u32 ms) {
    struct timeval tv = { .tv_sec = ms / 1000, .tv_usec = (suseconds_t)((ms % 1000) * 1000) };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// --- Client ---

static bool writeAll(i32 fd, const char *buf, bytes len) {
    bytes sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += (bytes)n;
    }
    return true;
}

// Read one CRLF-terminated line (without CRLF) from fd.
static bool readLine(i32 fd, char *out, bytes cap, bytes *lenOut) {
    bytes n = 0;
    while (n + 1 < cap) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);
        if (r <= 0) return false;
        if (c == '\n') {
            if (n > 0 && out[n - 1] == '\r') n--;
            out[n] = '\0';
            *lenOut = n;
            return true;
        }
        out[n++] = c;
    }
    return false;
}

static bool recvSome(i32 fd, char *dst, bytes cap, bytes *gotOut) {
    ssize_t r = recv(fd, dst, cap, 0);
    if (r <= 0) return false;
    *gotOut = (bytes)r;
    return true;
}

static bool hasCRLF(const char *s) {
    if (!s)
        return false;
    for (; *s; s++) {
        if (*s == '\r' || *s == '\n')
            return true;
    }
    return false;
}

bool Http_perform(const HttpRequest *req, HttpResponse *resp) {
    // Preserve the caller's receive buffer across the init wipe.
    char *bodyOut = resp ? HttpResponse_body(resp) : nullptr;
    bytes bodyCap = resp ? HttpResponse_bodyCap(resp) : 0;

    memset(resp, 0, sizeof(*resp));
    HttpResponse_setStatus(resp, -1);
    HttpResponse_setBody(resp, bodyOut);
    HttpResponse_setBodyCap(resp, bodyCap);

    if (!req || !HttpRequest_host(req) || !HttpResponse_body(resp))
        return false;

    const char *method = HttpRequest_method(req) ? HttpRequest_method(req) : "GET";
    const char *path = HttpRequest_path(req) ? HttpRequest_path(req) : "/";
    const char *scheme = HttpRequest_scheme(req) ? HttpRequest_scheme(req) : "http";
    // Fail closed: https:// requires a TLS backend (tls_apple / tls_curl).
    // Refuse instead of downgrading to plain text.
    if (strcasecmp(scheme, "https") == 0)
        return false;
    if (strcasecmp(scheme, "http") != 0)
        return false;
    // Fail closed on CRLF injection: untrusted method/path/host/header
    // must not smuggle extra requests into the head block.
    if (hasCRLF(method) || hasCRLF(path) || hasCRLF(HttpRequest_host(req)))
        return false;
    for (u32 i = 0; i < HttpRequest_headerCount(req) && i < HTTP_MAX_HEADERS; i++) {
        if (hasCRLF(HttpRequest_headers(req)[i].name) || hasCRLF(HttpRequest_headers(req)[i].value))
            return false;
    }
    i32 port = HttpRequest_port(req) > 0 ? HttpRequest_port(req) : Url_defaultPort(scheme);
    u32 timeoutMs = HttpRequest_timeoutMs(req) ? HttpRequest_timeoutMs(req) : RECV_TIMEOUT_DEFAULT_MS;

    // --- Resolve ---
    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%d", port);
    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = nullptr;
    if (getaddrinfo(HttpRequest_host(req), portStr, &hints, &res) != 0 || !res) return false;

    struct addrinfo addrInfo = *res;
    i32 fd = socket(addrInfo.ai_family, addrInfo.ai_socktype, addrInfo.ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return false;
    }
    applyTimeout(fd, timeoutMs);

    bool ok = connect(fd, addrInfo.ai_addr, addrInfo.ai_addrlen) == 0;
    freeaddrinfo(res);
    if (!ok) {
        close(fd);
        return false;
    }

    // --- Send request head ---
    char head[HEADER_BLOCK_CAP];
    i32 h = snprintf(head, sizeof(head),
                     "%s %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Connection: close\r\n"
                     "Content-Length: %zu\r\n",
                     method, path, HttpRequest_host(req), HttpRequest_bodyLen(req));
    if (h < 0 || (bytes)h >= sizeof(head)) {
        close(fd);
        return false;
    }

    for (u32 i = 0; i < HttpRequest_headerCount(req) && i < HTTP_MAX_HEADERS; i++) {
        i32 k = snprintf(head + h, sizeof(head) - (bytes)h, "%s: %s\r\n",
                         HttpRequest_headers(req)[i].name, HttpRequest_headers(req)[i].value);
        if (k < 0 || (bytes)k >= sizeof(head) - (bytes)h) {
            close(fd);
            return false;
        }
        h += k;
    }
    h += snprintf(head + h, sizeof(head) - (bytes)h, "\r\n");
    ok = writeAll(fd, head, (bytes)h);

    // --- Send body (skip for GET-style no-body verbs) ---
    if (ok && HttpRequest_body(req) && HttpRequest_bodyLen(req))
        ok = writeAll(fd, HttpRequest_body(req), HttpRequest_bodyLen(req));

    // --- Status line + headers ---
    char line[2048];
    bytes lineLen;
    if (!ok || !readLine(fd, line, sizeof(line), &lineLen)) {
        close(fd);
        return false;
    }
    if (strncmp(line, "HTTP/", 5) != 0) {
        close(fd);
        return false;
    }
    HttpResponse_setStatus(resp, atoi(line + 9)); // "HTTP/1.1 NNN ..."

    bool chunked = false;
    while (readLine(fd, line, sizeof(line), &lineLen) && lineLen > 0) {
        const char *ct = "Content-Type:";
        const char *te = "Transfer-Encoding:";
        if (strncasecmp(line, ct, strlen(ct)) == 0) {
            const char *v = line + strlen(ct);
            while (*v == ' ') v++;
            snprintf(HttpResponse_contentTypeMut(resp), sizeof ((HttpResponse){0}).contentType, "%s", v);
        } else if (strncasecmp(line, te, strlen(te)) == 0) {
            if (strcasestr(line, "chunked")) chunked = true;
        }
    }

    // --- Body ---
    bytes total = 0;
    ok = true;
    if (chunked) {
        // Each chunk: hex-size line, payload, CRLF; zero chunk terminates.
        while (true) {
            if (!readLine(fd, line, sizeof(line), &lineLen)) { ok = false; break; }
            bytes chunkLen = (bytes)strtoul(line, nullptr, 16);
            if (chunkLen == 0) break;
            if (total + chunkLen > HttpResponse_bodyCap(resp)) { ok = false; break; }
            bytes got = 0;
            while (got < chunkLen && ok) {
                bytes step;
                if (!recvSome(fd, HttpResponse_body(resp) + total + got, chunkLen - got, &step)) ok = false;
                else got += step;
            }
            total += got;
            char crlf[2];
            if (ok) ok = recv(fd, crlf, 2, 0) == 2;
        }
    } else {
        ssize_t r;
        while (total < HttpResponse_bodyCap(resp) && (r = recv(fd, HttpResponse_body(resp) + total, HttpResponse_bodyCap(resp) - total, 0)) > 0)
            total += (bytes)r;
    }
    close(fd);

    HttpResponse_setBodyLen(resp, total);
    if (total < HttpResponse_bodyCap(resp))
        HttpResponse_body(resp)[total] = '\0';
    HttpResponse_setOk(resp, ok);
    return ok;
}

fn Http_respond(i32 clientFd, i32 status, const char *contentType,
                  const char *body, bytes bodyLen) {
    const char *reason =
        status == 200 ? "OK" :
        status == 201 ? "Created" :
        status == 400 ? "Bad Request" :
        status == 404 ? "Not Found" :
        status == 405 ? "Method Not Allowed" :
        status == 500 ? "Internal Server Error" : "Unknown";

    char head[1024];
    i32 h = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     status, reason, contentType ? contentType : "text/plain", bodyLen);
    if (h < 0) return;
    if (!writeAll(clientFd, head, (bytes)h)) return;
    if (bodyLen) writeAll(clientFd, body, bodyLen);
}

// --- Micro server ---

static any acceptLoop(any arg) {
    HttpServer *server = arg;
    while (HttpServer_running(server)) {
        // Poll with a i16 budget instead of blocking in accept forever:
        // stopping must stay cooperative, since closing the listener does
        // not reliably wake a parked accept on every platform.
        struct pollfd pfd = { .fd = HttpServer_listenFd(server), .events = POLLIN };
        if (poll(&pfd, 1, 100) <= 0)
            continue;

        i32 clientFd = accept(HttpServer_listenFd(server), nullptr, nullptr);
        if (clientFd < 0) continue;

        applyTimeout(clientFd, RECV_TIMEOUT_DEFAULT_MS);

        HttpExchange ex;
        memset(&ex, 0, sizeof(ex));

        char line[2048];
        bytes lineLen = 0;
        bytes total = 0;
        bool haveHead = false;
        bytes contentLen = 0;

        // Slurp until end of headers, then Content-Length worth of body.
        while (total < SERVER_BODY_CAP &&
               readLine(clientFd, line, sizeof(line), &lineLen)) {
            if (lineLen == 0) break; // blank line ends headers
            if (!haveHead) {
                sscanf(line, "%15s %1023s", ex.method, ex.path);
                haveHead = true;
                continue;
            }
            if (strncasecmp(line, "Content-Length:", 15) == 0)
                contentLen = (bytes)strtoul(line + 15, nullptr, 10);
        }
        // Fail closed: attacker Content-Length must not overflow the fixed box.
        if (contentLen > SERVER_BODY_CAP) {
            Http_respond(clientFd, 400, "text/plain", "body too large", 14);
            close(clientFd);
            continue;
        }
        while (total < contentLen) {
            bytes want = contentLen - total;
            if (want > SERVER_BODY_CAP - total) want = SERVER_BODY_CAP - total;
            ssize_t r = recv(clientFd, s_serverBody + total, want, 0);
            if (r <= 0) break;
            total += (bytes)r;
        }

        ex.body = total ? s_serverBody : nullptr;
        ex.bodyLen = total;

        HttpServer_handler(server)(&ex, clientFd, HttpServer_userdata(server));
        close(clientFd);
    }
    return nullptr;
}

bool HttpServer_start(HttpServer *server, i32 port, HttpHandler handler,
                      any userdata, i32 *portOut) {
    memset(server, 0, sizeof(*server));
    HttpServer_setHandler(server, handler);
    HttpServer_setUserdata(server, userdata);

    i32 fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    i32 reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr = { .sin_family = AF_INET,
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                .sin_port = htons((u16)(port > 0 ? port : 0)) };
    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0 || listen(fd, 4) != 0) {
        close(fd);
        return false;
    }

    socklen_t len = sizeof(addr);
    getsockname(fd, (struct sockaddr*) &addr, &len);
    if (portOut) *portOut = ntohs(addr.sin_port);

    HttpServer_setListenFd(server, fd);
    HttpServer_setRunning(server, true);
    return pthread_create(HttpServer_threadMut(server), nullptr, acceptLoop, server) == 0;
}

fn HttpServer_stop(HttpServer *server) {
    if (!HttpServer_listenFd(server)) return;
    HttpServer_setRunning(server, false);      // loop notices within one poll period
    pthread_join(HttpServer_thread(server), nullptr);
    close(HttpServer_listenFd(server));
    HttpServer_setListenFd(server, 0);
}
