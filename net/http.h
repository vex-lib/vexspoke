#ifndef NET_HTTP_H
#define NET_HTTP_H

#include "vex.h"
#include <pthread.h>

// net/http.h — HTTP/1.1 client & micro server over BSD sockets
// (Legacy: net/HTTPClient.java, net/HTTPServer.java, net/PollRequest.java).
//
// One transport, two directions. The client performs one request per call
// (Connection: close semantics) into caller-owned buffers — nothing inside
// this module allocates. TLS is not wired yet: http:// only until a curl
// backend lands behind the same surface (the legacy used libcurl for https).

#define HTTP_MAX_HEADERS 16

typedef struct HttpHeader {
    const char *name;
    const char *value;
} HttpHeader;

typedef struct HttpRequest {
    const char *scheme;   // "http" default, "https" requires TLS backend
    const char *method;   // "GET", "POST", ... default "GET"
    const char *host;     // required
    int port;             // 0 => 80
    const char *path;     // default "/"
    const HttpHeader *headers;
    u32 headerCount;
    const char *body;     // nullptr = no body
    bytes bodyLen;
    u32 timeoutMs;   // connect+read budget, default 5000
} HttpRequest;

static inline HttpRequest HttpRequest_3(const char *host, int port, const char *path) {
    return (HttpRequest) { .scheme = "http", .method = "GET", .host = host,
        .port = port, .path = path, .timeoutMs = 5000u };
}
#define HttpRequest(...) CONSTRUCTOR_DISPATCH(HttpRequest, __VA_ARGS__)
static inline fn HttpRequest_setScheme(HttpRequest *request, const char *scheme) {
    if (request) (*request).scheme = scheme;
}
static inline fn HttpRequest_setMethod(HttpRequest *request, const char *method) {
    if (request) (*request).method = method;
}
static inline fn HttpRequest_setHeaders(HttpRequest *request,
                                          const HttpHeader *headers, u32 count) {
    if (request) { (*request).headers = headers; (*request).headerCount = count; }
}
static inline fn HttpRequest_setBody(HttpRequest *request, const char *body, bytes length) {
    if (request) { (*request).body = body; (*request).bodyLen = length; }
}
static inline fn HttpRequest_setTimeout(HttpRequest *request, u32 timeoutMilliseconds) {
    if (request) (*request).timeoutMs = timeoutMilliseconds;
}

typedef struct HttpResponse {
    bool ok;              // transport-level success; status may still be 4xx
    int status;
    char contentType[128];
    char *body;           // caller-owned receive buffer
    bytes bodyCap;
    bytes bodyLen;
} HttpResponse;

static inline HttpResponse HttpResponse_2(char *body, bytes capacity) {
    return (HttpResponse) { .status = -1, .body = body, .bodyCap = capacity };
}
#define HttpResponse(...) CONSTRUCTOR_DISPATCH(HttpResponse, __VA_ARGS__)

// Execute one request. Fills resp; body is written into (*resp).body up to
// bodyCap and NUL-terminated when it fits.
bool Http_perform(const HttpRequest *req, HttpResponse *resp);

// --- Micro server ---

// What the handler sees of one exchange. body points into an internal
// buffer valid only during the callback.
typedef struct HttpExchange {
    char method[16];
    char path[1024];
    const char *body;
    bytes bodyLen;
} HttpExchange;

typedef fn (*HttpHandler)(const HttpExchange *exchange, int clientFd, any userdata);

typedef struct HttpServer {
    int listenFd;
    pthread_t thread;
    volatile bool running;
    HttpHandler handler;
    any userdata;
} HttpServer;

// Bind + listen + spawn the accept loop on its own thread. Returns false on
// socket failure; portOut receives the bound port (use 0 to pick ephemeral).
bool HttpServer_start(HttpServer *server, int port, HttpHandler handler,
                      any userdata, int *portOut);

// Stop accepting and close the listener. In-flight handler finishes.
fn HttpServer_stop(HttpServer *server);

// Handler-side responder: writes a full response on the open connection.
fn Http_respond(int clientFd, int status, const char *contentType,
                  const char *body, bytes bodyLen);

#endif
