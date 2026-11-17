#include "vex.h"
#include "net/websocket.h"
#include "oop/vexspoke_type.h"

#include "lang/str.h"
#include "net/url.h"
#include "re/nio/mem.h"
#include "security/secure_random.h"
#include "security/sha1.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: WebSocket (net/websocket.c)
 * ============================================================================
 * RFC 6455 in one client connection. Vexspoke `net/` owns the ecosystem's
 * network protocols AND their socket adapters (the same arrangement net/http.c
 * already has), so the protocol and the BSD socket it rides on both live here.
 * R3 never includes this header: api-haven owns the `WsSource` fn-table SHAPE
 * and the fan-out/SSE state machines, and a host that can see both layers binds
 * this client to that shape. That keeps one owner per seam and no reverse
 * include.
 *
 * The class is caller-stored, like HttpResponse: WebSocket() yields the empty
 * instance and WebSocket_open() fills it, so construction allocates nothing on
 * the caller's behalf. The ONE buffer this class owns is the reassembly buffer
 * for a fragmented data message, taken from the engine's Memory_ ABI (it owns
 * its storage; Memory_free releases it). No thread is created and no wait is
 * unbounded: every read and write is bounded by the caller's transport budget,
 * the frame loop runs in bounded slices and re-checks that budget, and a stall
 * mid-frame fails the connection instead of blocking forever (the Bounded Wait
 * Law).
 *
 * The handshake is the RFC's: GET with Upgrade/Connection, a fresh
 * Sec-WebSocket-Key of 16 random Bytes, the version 13 pin, and verification of
 * `Sec-WebSocket-Accept` against base64(SHA1(key || GUID)). A peer that answers
 * anything else is refused loudly and the connection is dropped. Frames are
 * strict: reserved bits must be zero, a 64-bit length must clear its high bit, a
 * server frame must not be masked, control frames must be final and within 125
 * Bytes, continuation frames must follow a start frame, and a text message must
 * be valid UTF-8. Every one of those is a refusal with a bounded diagnostic
 * rather than a guess, and the connection moves to FAILED so no later call can
 * act on a desynchronized stream.
 *
 * `wss://` is deliberately refused until a TLS backend lands behind net/tls.h;
 * claiming transport security without it would be worse than not offering it.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: WebSocket (net/websocket.c)
 * ============================================================================
 * RFC 6455 client: handshake, frame codec with client masking, control path and
 * continuation reassembly, over one BSD socket owned by this class.
 *
 * STRUCT FIELDS (Mirroring net/websocket.h):
 * ----------------------------------------------------------------------------
 *   WebSocket {
 *     i32 fd;                        // connected socket, -1 once released
 *     WebSocketState state;          // CLOSED/OPEN/CLOSING/FAILED
 *     u32 timeoutMs;            // transport budget for one read/write burst
 *     bytes maxMessageBytes;        // reassembly cap
 *     u8 *message;              // owned reassembly buffer
 *     bytes messageCap;             // allocated Bytes
 *     bytes messageLength;          // Bytes accumulated for the current message
 *     u32 messageOpcode;        // TEXT/BINARY while fragments accumulate
 *     u8 control[125];          // ping/pong/close payload
 *     u16 closeCode;            // finalized close code (1005 when absent)
 *     char closeReason[128];         // bounded close reason text
 *     bool closeReasonTruncated;     // the reason was cut to fit
 *     char error[128];               // bounded last-failure text
 *     u64 framesIn;             // complete frames decoded
 *     u64 framesOut;            // complete frames encoded and sent
 *     u64 pongsSent;            // automatic pong replies
 *   }
 *
 * PRIVATE HELPER RECORD (kept file-local, behaviorless):
 *   WsTarget {
 *     char host[256];  // authority host (userinfo stripped)
 *     char path[2048]; // request target, "/" when absent
 *     i32 port;        // 80 when the authority omits it
 *   }
 *
 * PRIVATE HELPERS (file-local, pure logic):
 *   wsNowMs()                          // monotonic milliseconds (static)
 *   wsSetTimeout(fd, ms)               // SO_RCVTIMEO/SO_SNDTIMEO (static)
 *   wsConnect(target, ms)              // bounded non-blocking connect (static)
 *   wsSendAll(fd, bytes, length)       // i16-write-safe (static)
 *   wsReadExact(self, dst, length)     // i16-read-safe, timeout-bounded (static)
 *   wsFindHeaderEnd(bytes, length)     // "\r\n\r\n" end index (static)
 *   wsHeaderValue(block, name, out, cap) // case-insensitive header fetch (static)
 *   wsParseUrl(url, out)               // ws:// authority/path split (static)
 *   wsFail(self, reason)               // record text + FAILED (static)
 *   wsUtf8Valid(bytes, length)         // RFC 3629 validator (static)
 *   wsAppendMessage(self, bytes, n)    // bounded reassembly append (static)
 *   wsSendFrame(self, op, fin, p, n)   // encode + fresh mask + send (static)
 *   wsReadFrameHeader(self, ...)       // decode the frame header (static)
 *   wsConsumeFrame(self)               // one frame -> message record (static)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:        WebSocket_0() (inline, in the header)
 * Public Core:         WebSocket_open / _close / _poll
 *                      _writeText / _writeBinary / _writePing / _writeClose
 * Public Getters:      _state / _isOpen / _lastError / _closeCode / _closeReason
 *                      _maxMessageBytes / _framesIn / _framesOut / _pongsSent
 * Public Projections:  _toString / _toStringStruct
 * CONTRACT: WebSocket() yields the empty instance; a null self is safe on every
 * getter and projection. open() connects and handshakes, leaving the client
 * OPEN, or returns false with the client CLOSED/FAILED and a bounded diagnostic.
 * Every failure inside the frame loop moves the client to FAILED, which is
 * terminal: poll() reports it and no further frame is trusted. poll() never
 * blocks longer than its budget, answers pings itself, and returns a record
 * whose payload borrows the reassembly buffer until the next poll or close.
 * close() sends a normal close when still open, releases the socket and the
 * owned buffer, and is idempotent. One connection is caller-serialized: the
 * class holds no lock and is not safe for concurrent callers.
 * ============================================================================
 */


;;GETTER
static inline u16 WebSocket_closeCode(const WebSocket *s) { return (*s).closeCode; }
;;GETTER
static inline u16 *WebSocket_closeCodeMut(WebSocket *s) { return &(*s).closeCode; }
;;SETTER
static inline fn WebSocket_setCloseCode(WebSocket *s, u16 v) { (*s).closeCode = v; }
;;GETTER
static inline char *WebSocket_closeReasonMut(WebSocket *s) { return (*s).closeReason; }
;;GETTER
static inline const char *WebSocket_closeReason(const WebSocket *s) { return (*s).closeReason; }
;;GETTER
static inline bool WebSocket_closeReasonTruncated(const WebSocket *s) { return (*s).closeReasonTruncated; }
;;GETTER
static inline bool *WebSocket_closeReasonTruncatedMut(WebSocket *s) { return &(*s).closeReasonTruncated; }
;;SETTER
static inline fn WebSocket_setCloseReasonTruncated(WebSocket *s, bool v) { (*s).closeReasonTruncated = v; }
;;GETTER
static inline u8 *WebSocket_controlMut(WebSocket *s) { return (*s).control; }
;;GETTER
static inline const u8 *WebSocket_control(const WebSocket *s) { return (*s).control; }
;;GETTER
static inline char *WebSocket_errorMut(WebSocket *s) { return (*s).error; }
;;GETTER
static inline const char *WebSocket_error(const WebSocket *s) { return (*s).error; }
;;GETTER
static inline i32 WebSocket_fd(const WebSocket *s) { return (*s).fd; }
;;GETTER
static inline i32 *WebSocket_fdMut(WebSocket *s) { return &(*s).fd; }
;;SETTER
static inline fn WebSocket_setFd(WebSocket *s, i32 v) { (*s).fd = v; }
;;GETTER
static inline u64 WebSocket_framesIn(const WebSocket *s) { return (*s).framesIn; }
;;GETTER
static inline u64 *WebSocket_framesInMut(WebSocket *s) { return &(*s).framesIn; }
;;SETTER
static inline fn WebSocket_setFramesIn(WebSocket *s, u64 v) { (*s).framesIn = v; }
;;GETTER
static inline u64 WebSocket_framesOut(const WebSocket *s) { return (*s).framesOut; }
;;GETTER
static inline u64 *WebSocket_framesOutMut(WebSocket *s) { return &(*s).framesOut; }
;;SETTER
static inline fn WebSocket_setFramesOut(WebSocket *s, u64 v) { (*s).framesOut = v; }
;;GETTER
static inline bytes WebSocket_maxMessageBytes(const WebSocket *s) { return (*s).maxMessageBytes; }
;;GETTER
static inline bytes *WebSocket_maxMessageBytesMut(WebSocket *s) { return &(*s).maxMessageBytes; }
;;SETTER
static inline fn WebSocket_setMaxMessageBytes(WebSocket *s, bytes v) { (*s).maxMessageBytes = v; }
;;GETTER
static inline u8 * WebSocket_message(const WebSocket *s) { return (*s).message; }
;;GETTER
static inline u8 * *WebSocket_messageMut(WebSocket *s) { return &(*s).message; }
;;SETTER
static inline fn WebSocket_setMessage(WebSocket *s, u8 * v) { (*s).message = v; }
;;GETTER
static inline bytes WebSocket_messageCap(const WebSocket *s) { return (*s).messageCap; }
;;GETTER
static inline bytes *WebSocket_messageCapMut(WebSocket *s) { return &(*s).messageCap; }
;;SETTER
static inline fn WebSocket_setMessageCap(WebSocket *s, bytes v) { (*s).messageCap = v; }
;;GETTER
static inline bytes WebSocket_messageLength(const WebSocket *s) { return (*s).messageLength; }
;;GETTER
static inline bytes *WebSocket_messageLengthMut(WebSocket *s) { return &(*s).messageLength; }
;;SETTER
static inline fn WebSocket_setMessageLength(WebSocket *s, bytes v) { (*s).messageLength = v; }
;;GETTER
static inline u32 WebSocket_messageOpcode(const WebSocket *s) { return (*s).messageOpcode; }
;;GETTER
static inline u32 *WebSocket_messageOpcodeMut(WebSocket *s) { return &(*s).messageOpcode; }
;;SETTER
static inline fn WebSocket_setMessageOpcode(WebSocket *s, u32 v) { (*s).messageOpcode = v; }
;;GETTER
static inline u64 WebSocket_pongsSent(const WebSocket *s) { return (*s).pongsSent; }
;;GETTER
static inline u64 *WebSocket_pongsSentMut(WebSocket *s) { return &(*s).pongsSent; }
;;SETTER
static inline fn WebSocket_setPongsSent(WebSocket *s, u64 v) { (*s).pongsSent = v; }
;;GETTER
static inline WebSocketState WebSocket_state(const WebSocket *s) { return (*s).state; }
;;GETTER
static inline WebSocketState *WebSocket_stateMut(WebSocket *s) { return &(*s).state; }
;;SETTER
static inline fn WebSocket_setState(WebSocket *s, WebSocketState v) { (*s).state = v; }
;;GETTER
static inline u32 WebSocket_timeoutMs(const WebSocket *s) { return (*s).timeoutMs; }
;;GETTER
static inline u32 *WebSocket_timeoutMsMut(WebSocket *s) { return &(*s).timeoutMs; }
;;SETTER
static inline fn WebSocket_setTimeoutMs(WebSocket *s, u32 v) { (*s).timeoutMs = v; }
;;GETTER
static inline TlsConn * WebSocket_tls(const WebSocket *s) { return (*s).tls; }
;;GETTER
static inline TlsConn * *WebSocket_tlsMut(WebSocket *s) { return &(*s).tls; }
;;SETTER
static inline fn WebSocket_setTls(WebSocket *s, TlsConn * v) { (*s).tls = v; }

// PRIVATE HELPER RECORD

typedef struct WsTarget {
    char host[WS_HOST_BYTES]; // authority host (userinfo stripped)
    char path[WS_PATH_BYTES]; // request target, "/" when absent
    i32 port;                 // 80 for ws://, 443 for wss://, when omitted
    bool secure;              // true for wss://, decided by the scheme alone
} WsTarget;

;;GETTER
static inline char *WsTarget_hostMut(WsTarget *s) { return (*s).host; }
;;GETTER
static inline const char *WsTarget_host(const WsTarget *s) { return (*s).host; }
;;GETTER
static inline char *WsTarget_pathMut(WsTarget *s) { return (*s).path; }
;;GETTER
static inline const char *WsTarget_path(const WsTarget *s) { return (*s).path; }
;;GETTER
static inline i32 WsTarget_port(const WsTarget *s) { return (*s).port; }
;;GETTER
static inline i32 *WsTarget_portMut(WsTarget *s) { return &(*s).port; }
;;SETTER
static inline fn WsTarget_setPort(WsTarget *s, i32 v) { (*s).port = v; }
;;GETTER
static inline bool WsTarget_secure(const WsTarget *s) { return (*s).secure; }
;;GETTER
static inline bool *WsTarget_secureMut(WsTarget *s) { return &(*s).secure; }
;;SETTER
static inline fn WsTarget_setSecure(WsTarget *s, bool v) { (*s).secure = v; }


// PRIVATE HELPERS

/** Monotonic milliseconds; the only clock the frame loop trusts. */
static u64 wsNowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64) ts.tv_sec * 1000u + (u64) (ts.tv_nsec / 1000000);
}

/** Bound both directions of one socket by the same transport budget. */
static fn wsSetTimeout(i32 fd, u32 timeoutMs) {
    struct timeval tv;
    tv.tv_sec = (time_t) (timeoutMs / 1000u);
    tv.tv_usec = (suseconds_t) ((timeoutMs % 1000u) * 1000u);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/**
 * Connect with a bounded wait: a non-blocking connect plus a poll armed with the
 * caller's budget, so an unreachable host consumes that budget and no more.
 */
static i32 wsConnect(const WsTarget *target, u32 timeoutMs) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char service[8];
    snprintf(service, sizeof(service), "%d", WsTarget_port(target));
    struct addrinfo *resolved = nullptr;
    if (getaddrinfo(WsTarget_hostMut(target), service, &hints, &resolved) != 0 ||
        resolved == nullptr)
        return -1;

    i32 fd = -1;
    for (struct addrinfo *it = resolved; it != nullptr;) {
        struct addrinfo addrInfo = *it;
        it = addrInfo.ai_next;
        fd = socket(addrInfo.ai_family, addrInfo.ai_socktype, addrInfo.ai_protocol);
        if (fd < 0)
            continue;
        i32 flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(fd);
            fd = -1;
            continue;
        }
        i32 state = connect(fd, addrInfo.ai_addr, addrInfo.ai_addrlen);
        if (state != 0 && errno == EINPROGRESS) {
            struct pollfd waiting = { .fd = fd, .events = POLLOUT };
            if (poll(&waiting, 1, (i32) timeoutMs) > 0) {
                i32 socketError = 0;
                socklen_t errorLength = (socklen_t) sizeof(socketError);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socketError, &errorLength) == 0 &&
                    socketError == 0)
                    state = 0;
            }
        }
        if (state == 0) {
            fcntl(fd, F_SETFL, flags);
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(resolved);
    if (fd >= 0)
        wsSetTimeout(fd, timeoutMs);
    return fd;
}

/** Write a whole buffer, retrying i16 writes. */
static bool wsSendAll(i32 fd, const u8 *data, bytes length) {
    bytes sent = 0u;
    while (sent < length) {
        ssize_t written = send(fd, data + sent, length - sent, 0);
        if (written <= 0)
            return false;
        sent += (bytes) written;
    }
    return true;
}

/** Write through whichever transport this client owns (socket or TLS). */
static bool wsWrite(WebSocket *self, const u8 *data, bytes length) {
    if (self == nullptr)
        return false;
    if (WebSocket_tls(self) != nullptr)
        return Tls_send(WebSocket_tls(self), (const char*) data, length);
    return WebSocket_fd(self) >= 0 && wsSendAll(WebSocket_fd(self), data, length);
}

/** Read exactly length Bytes, retrying i16 reads within the transport budget. */
static bool wsReadExact(WebSocket *self, u8 *dest, bytes length) {
    bytes got = 0u;
    while (got < length) {
        if (WebSocket_tls(self) != nullptr) {
            bytes taken = 0u;
            if (!Tls_recv(WebSocket_tls(self), (char*) dest + got, length - got, &taken) ||
                taken == 0u)
                return false;
            got += taken;
            continue;
        }
        ssize_t taken = recv(WebSocket_fd(self), dest + got, length - got, 0);
        if (taken <= 0)
            return false;
        got += (bytes) taken;
    }
    return true;
}

/** Index just past a complete "\r\n\r\n", or SIZE_MAX when it is absent. */
static bytes wsFindHeaderEnd(const char *data, bytes length) {
    for (bytes i = 3u; i < length; i++) {
        if (data[i - 3u] == '\r' && data[i - 2u] == '\n' &&
            data[i - 1u] == '\r' && data[i] == '\n')
            return i + 1u;
    }
    return SIZE_MAX;
}

/** Case-insensitive ASCII compare of n Bytes. */
static bool wsCiEq(const char *a, const char *b, bytes n) {
    for (bytes i = 0u; i < n; i++) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char) (ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char) (cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return true;
}

/**
 * Copy one response header's trimmed value into out, or return false when the
 * header is absent or the value would not fit. Names are matched
 * case-insensitively, as the RFC requires.
 */
static bool wsHeaderValue(const char *block, const char *name, char *out, bytes cap) {
    bytes nameLength = strlen(name);
    const char *line = block;
    while (line != nullptr && *line != '\0') {
        const char *end = strstr(line, "\r\n");
        const char *stop = end == nullptr ? line + strlen(line) : end;
        const char *colon = memchr(line, ':', (bytes) (stop - line));
        if (colon != nullptr && (bytes) (colon - line) == nameLength &&
            wsCiEq(line, name, nameLength)) {
            const char *value = colon + 1;
            while (value < stop && (*value == ' ' || *value == '\t'))
                value++;
            const char *tail = stop;
            while (tail > value && (tail[-1] == ' ' || tail[-1] == '\t'))
                tail--;
            bytes valueLength = (bytes) (tail - value);
            if (valueLength + 1u > cap)
                return false;
            memcpy(out, value, valueLength);
            out[valueLength] = '\0';
            return true;
        }
        if (end == nullptr)
            break;
        line = end + 2;
    }
    return false;
}

/** Split a ws:// URL into authority host, port and request target. */
static bool wsParseUrl(const char *url, WsTarget *out) {
    if (url == nullptr)
        return false;
    bool secure = false;
    const char *rest = nullptr;
    if (strncmp(url, "wss://", 6u) == 0) {
        secure = true;
        rest = url + 6u;
    } else if (strncmp(url, "ws://", 5u) == 0) {
        rest = url + 5u;
    } else {
        return false;
    }
    const char *slash = strchr(rest, '/');
    bytes authorityLength = slash == nullptr ? strlen(rest) : (bytes) (slash - rest);
    if (authorityLength == 0u || authorityLength >= WS_HOST_BYTES + 8u)
        return false;
    char authority[WS_HOST_BYTES + 8u];
    memcpy(authority, rest, authorityLength);
    authority[authorityLength] = '\0';

    const char *userInfo = strrchr(authority, '@');
    const char *hostPort = userInfo == nullptr ? authority : userInfo + 1;

    i32 port = secure ? 443 : 80;
    bytes hostLength = strlen(hostPort);
    if (*hostPort == '[') {
        const char *close = strchr(hostPort, ']');
        if (close == nullptr)
            return false;
        hostLength = (bytes) (close - hostPort - 1);
        const char *after = close + 1;
        if (*after != '\0') {
            if (*after != ':')
                return false;
            i32 value = 0;
            for (const char *digit = after + 1; *digit != '\0'; digit++) {
                if (*digit < '0' || *digit > '9')
                    return false;
                value = value * 10 + (*digit - '0');
                if (value > 65535)
                    return false;
            }
            port = value;
        }
        if (hostLength == 0u || hostLength >= WS_HOST_BYTES)
            return false;
        memset(out, 0, sizeof(*out));
        memcpy(WsTarget_hostMut(out), hostPort + 1, hostLength);
        WsTarget_hostMut(out)[hostLength] = '\0';
    } else {
        const char *colon = strrchr(hostPort, ':');
        if (colon != nullptr) {
            hostLength = (bytes) (colon - hostPort);
            const char *digits = colon + 1;
            if (*digits == '\0')
                return false;
            i32 value = 0;
            for (const char *digit = digits; *digit != '\0'; digit++) {
                if (*digit < '0' || *digit > '9')
                    return false;
                value = value * 10 + (*digit - '0');
                if (value > 65535)
                    return false;
            }
            port = value;
        }
        if (hostLength == 0u || hostLength >= WS_HOST_BYTES)
            return false;
        memset(out, 0, sizeof(*out));
        memcpy(WsTarget_hostMut(out), hostPort, hostLength);
        WsTarget_hostMut(out)[hostLength] = '\0';
    }

    const char *path = slash == nullptr ? "/" : slash;
    if (strlen(path) >= WS_PATH_BYTES)
        return false;
    memcpy(WsTarget_pathMut(out), path, strlen(path) + 1u);
    WsTarget_setPort(out, port);
    WsTarget_setSecure(out, secure);
    return true;
}

/** Record a bounded diagnostic and make the connection terminal. */
static fn wsFail(WebSocket *self, const char *reason) {
    if (self == nullptr)
        return;
    bytes cap = sizeof ((WebSocket){0}).error;
    bytes length = strlen(reason);
    bytes copy = length < cap - 1u ? length : cap - 1u;
    memcpy(WebSocket_errorMut(self), reason, copy);
    WebSocket_errorMut(self)[copy] = '\0';
    WebSocket_setState(self, WEBSOCKET_FAILED);
}

/** RFC 3629 validation: rejects overlong forms and surrogate halves. */
static bool wsUtf8Valid(const u8 *data, bytes length) {
    bytes i = 0u;
    while (i < length) {
        u8 lead = data[i];
        if (lead < 0x80u) {
            i++;
            continue;
        }
        bytes extra;
        u32 codepoint;
        u32 minimum;
        if ((lead & 0xE0u) == 0xC0u) {
            extra = 1u;
            codepoint = lead & 0x1Fu;
            minimum = 0x80u;
        } else if ((lead & 0xF0u) == 0xE0u) {
            extra = 2u;
            codepoint = lead & 0x0Fu;
            minimum = 0x800u;
        } else if ((lead & 0xF8u) == 0xF0u) {
            extra = 3u;
            codepoint = lead & 0x07u;
            minimum = 0x10000u;
        } else {
            return false;
        }
        if (i + extra >= length)
            return false;
        for (bytes k = 1u; k <= extra; k++) {
            if ((data[i + k] & 0xC0u) != 0x80u)
                return false;
            codepoint = (codepoint << 6) | (u32) (data[i + k] & 0x3Fu);
        }
        if (codepoint < minimum || codepoint > 0x10FFFFu ||
            (codepoint >= 0xD800u && codepoint <= 0xDFFFu))
            return false;
        i += extra + 1u;
    }
    return true;
}

/** Append reassembled payload Bytes, refusing anything past the message cap. */
static bool wsAppendMessage(WebSocket *self, const u8 *data, bytes length) {
    if (length > WebSocket_maxMessageBytes(self) - WebSocket_messageLength(self)) {
        wsFail(self, "message exceeds the configured cap");
        return false;
    }
    bytes wanted = WebSocket_messageLength(self) + length;
    if (wanted > WebSocket_messageCap(self)) {
        bytes cap = WebSocket_messageCap(self) == 0u ? 1024u : WebSocket_messageCap(self);
        while (cap < wanted)
            cap *= 2u;
        u8 *grown = (u8*) Memory_realloc(WebSocket_message(self), cap);
        if (grown == nullptr) {
            wsFail(self, "message buffer growth failed");
            return false;
        }
        WebSocket_setMessage(self, grown);
        WebSocket_setMessageCap(self, cap);
    }
    memcpy(WebSocket_message(self) + WebSocket_messageLength(self), data, length);
    WebSocket_setMessageLength(self, WebSocket_messageLength(self) + (length));
    return true;
}

/** True when this client owns a live transport — a socket or a TLS session. */
static inline bool wsConnected(const WebSocket *self) {
    return self != nullptr && (WebSocket_fd(self) >= 0 || WebSocket_tls(self) != nullptr);
}

/** Encode one final-or-fragmented frame with a fresh mask and send it. */
static bool wsSendFrame(WebSocket *self, u8 opcode, bool fin,
                        const u8 *payload, bytes length) {
    if (!wsConnected(self))
        return false;
    u8 header[14];
    bytes headerLength = 0u;
    header[headerLength++] = (u8) ((fin ? 0x80u : 0x00u) | (opcode & 0x0Fu));
    if (length <= 125u) {
        header[headerLength++] = (u8) (0x80u | length);
    } else if (length <= 0xFFFFu) {
        header[headerLength++] = (u8) (0x80u | 126u);
        header[headerLength++] = (u8) (length >> 8);
        header[headerLength++] = (u8) (length & 0xFFu);
    } else {
        header[headerLength++] = (u8) (0x80u | 127u);
        u64 value = (u64) length;
        for (i32 shift = 56; shift >= 0; shift -= 8)
            header[headerLength++] = (u8) ((value >> shift) & 0xFFu);
    }
    u8 mask[4];
    SecureRandom *random = SecureRandom_0();
    if (random == nullptr)
        return false;
    bool masked = SecureRandom_bytes(random, mask, sizeof(mask));
    SecureRandom_free(random);
    if (!masked)
        return false;
    memcpy(header + headerLength, mask, 4u);
    headerLength += 4u;
    if (!wsWrite(self, header, headerLength))
        return false;

    // Mask in bounded chunks: the mask index keeps running across chunks, so
    // chunking cannot change a single payload Byte.
    u8 chunk[WS_RX_BYTES_DEFAULT];
    bytes sent = 0u;
    while (sent < length) {
        bytes take = length - sent < sizeof(chunk) ? length - sent : sizeof(chunk);
        for (bytes i = 0u; i < take; i++)
            chunk[i] = (u8) (payload[sent + i] ^ mask[(sent + i) & 3u]);
        if (!wsWrite(self, chunk, take))
            return false;
        sent += take;
    }
    WebSocket_setFramesOut(self, WebSocket_framesOut(self) + 1u);
    return true;
}

/** Decode one frame header, refusing reserved bits and a bad 64-bit length. */
static bool wsReadFrameHeader(WebSocket *self, bool *fin, u8 *opcode,
                              bool *masked, u64 *length) {
    u8 head[2];
    if (!wsReadExact(self, head, 2u)) {
        wsFail(self, "frame header read failed");
        return false;
    }
    if ((head[0] & 0x70u) != 0u) {
        wsFail(self, "reserved frame bits must be zero");
        return false;
    }
    *fin = (head[0] & 0x80u) != 0u;
    *opcode = (u8) (head[0] & 0x0Fu);
    *masked = (head[1] & 0x80u) != 0u;
    u64 value = (u64) (head[1] & 0x7Fu);
    if (value == 126u) {
        u8 extended[2];
        if (!wsReadExact(self, extended, 2u)) {
            wsFail(self, "frame length read failed");
            return false;
        }
        value = ((u64) extended[0] << 8) | (u64) extended[1];
    } else if (value == 127u) {
        u8 extended[8];
        if (!wsReadExact(self, extended, 8u)) {
            wsFail(self, "frame length read failed");
            return false;
        }
        if ((extended[0] & 0x80u) != 0u) {
            wsFail(self, "64-bit frame length must clear its high bit");
            return false;
        }
        value = 0u;
        for (bytes i = 0u; i < 8u; i++)
            value = (value << 8) | (u64) extended[i];
    }
    *length = value;
    return true;
}

/** Consume one frame: control frames act inline, data frames assemble. */
static WebSocketMessage wsConsumeFrame(WebSocket *self) {
    WebSocketMessage result;
    memset(&result, 0, sizeof(result));
    result.kind = WS_MESSAGE_NONE;
    result.closeCode = 1005u;

    bool fin = false;
    bool masked = false;
    u8 opcode = 0u;
    u64 length = 0u;
    if (!wsReadFrameHeader(self, &fin, &opcode, &masked, &length)) {
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }
    if (masked) {
        wsFail(self, "server frames must not be masked");
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }
    if (length > WebSocket_maxMessageBytes(self)) {
        wsFail(self, "frame length exceeds the configured cap");
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }

    if ((opcode & 0x08u) != 0u) {
        if (!fin || length > WS_CONTROL_MAX_BYTES) {
            wsFail(self, "control frames must be final and within 125 Bytes");
            result.kind = WS_MESSAGE_ERROR;
            memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
            return result;
        }
        if (length > 0u && !wsReadExact(self, WebSocket_controlMut(self), (bytes) length)) {
            wsFail(self, "control payload read failed");
            result.kind = WS_MESSAGE_ERROR;
            memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
            return result;
        }
        WebSocket_setFramesIn(self, WebSocket_framesIn(self) + 1u);
        if (opcode == WEBSOCKET_PING) {
            if (!wsSendFrame(self, WEBSOCKET_PONG, true, WebSocket_controlMut(self), (bytes) length)) {
                wsFail(self, "pong reply failed");
                result.kind = WS_MESSAGE_ERROR;
                memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
                return result;
            }
            WebSocket_setPongsSent(self, WebSocket_pongsSent(self) + 1u);
            return result;
        }
        if (opcode == WEBSOCKET_PONG)
            return result;
        if (opcode == WEBSOCKET_CLOSE) {
            u16 code = 1005u;
            if (length >= 2u)
                code = (u16) (((u16) WebSocket_controlMut(self)[0] << 8) |
                                   (u16) WebSocket_controlMut(self)[1]);
            WebSocket_setCloseCode(self, code);
            WebSocket_closeReasonMut(self)[0] = '\0';
            WebSocket_setCloseReasonTruncated(self, false);
            if (length > 2u) {
                bytes textLength = (bytes) length - 2u;
                bytes cap = sizeof ((WebSocket){0}).closeReason - 1u;
                bytes copy = textLength < cap ? textLength : cap;
                memcpy(WebSocket_closeReasonMut(self), WebSocket_controlMut(self) + 2u, copy);
                WebSocket_closeReasonMut(self)[copy] = '\0';
                WebSocket_setCloseReasonTruncated(self, copy < textLength);
            }
            if (WebSocket_state(self) == WEBSOCKET_OPEN)
                (discard) wsSendFrame(self, WEBSOCKET_CLOSE, true, WebSocket_controlMut(self), (bytes) length);
            WebSocket_setState(self, WEBSOCKET_CLOSED);
            result.kind = WS_MESSAGE_CLOSE;
            result.closeCode = code;
            result.truncated = WebSocket_closeReasonTruncated(self);
            return result;
        }
        wsFail(self, "unknown control opcode");
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }

    if (opcode == WEBSOCKET_CONTINUATION) {
        if (WebSocket_messageOpcode(self) == 0u) {
            wsFail(self, "continuation frame without a start frame");
            result.kind = WS_MESSAGE_ERROR;
            memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
            return result;
        }
    } else if (opcode == WEBSOCKET_TEXT || opcode == WEBSOCKET_BINARY) {
        if (WebSocket_messageOpcode(self) != 0u) {
            wsFail(self, "new data frame inside a fragmented message");
            result.kind = WS_MESSAGE_ERROR;
            memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
            return result;
        }
        WebSocket_setMessageOpcode(self, opcode);
        WebSocket_setMessageLength(self, 0u);
    } else {
        wsFail(self, "unknown data opcode");
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }

    bytes remaining = (bytes) length;
    u8 chunk[WS_RX_BYTES_DEFAULT];
    while (remaining > 0u) {
        bytes take = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        if (!wsReadExact(self, chunk, take) || !wsAppendMessage(self, chunk, take)) {
            if (WebSocket_state(self) != WEBSOCKET_FAILED)
                wsFail(self, "message payload read failed");
            result.kind = WS_MESSAGE_ERROR;
            memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
            return result;
        }
        remaining -= take;
    }
    WebSocket_setFramesIn(self, WebSocket_framesIn(self) + 1u);
    if (!fin)
        return result; // a fragmented message continues on the next frame

    if (WebSocket_messageOpcode(self) == WEBSOCKET_TEXT &&
        !wsUtf8Valid(WebSocket_message(self), WebSocket_messageLength(self))) {
        wsFail(self, "text message is not valid UTF-8");
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }
    result.kind = WebSocket_messageOpcode(self) == WEBSOCKET_TEXT ? WS_MESSAGE_TEXT
                                                         : WS_MESSAGE_BINARY;
    result.payload = WebSocket_message(self);
    result.length = WebSocket_messageLength(self);
    WebSocket_setMessageOpcode(self, 0u);
    WebSocket_setMessageLength(self, 0u);
    return result;
}

/** Perform the RFC 6455 opening handshake and verify the accept hash. */
static bool wsHandshake(WebSocket *self, const WsTarget *target) {
    u8 keyBytes[16];
    SecureRandom *random = SecureRandom_0();
    if (random == nullptr || !SecureRandom_bytes(random, keyBytes, sizeof(keyBytes))) {
        SecureRandom_free(random);
        THROW("WebSocket_open: no secure random source for the client key");
        wsFail(self, "no secure random source");
        return false;
    }
    SecureRandom_free(random);

    char key[32];
    if (Url_base64(keyBytes, sizeof(keyBytes), key, sizeof(key)) < 0) {
        THROW("WebSocket_open: client key encoding failed");
        wsFail(self, "client key encoding failed");
        return false;
    }
    CryptoSha1 digest = CryptoSha1_0();
    Crypto_sha1Init(&digest);
    Crypto_sha1Update(&digest, key, strlen(key));
    Crypto_sha1Update(&digest, WS_HANDSHAKE_GUID, strlen(WS_HANDSHAKE_GUID));
    u8 hash[CRYPTO_SHA1_DIGEST_SIZE];
    Crypto_sha1Final(&digest, hash);
    char accept[64];
    if (Url_base64(hash, CRYPTO_SHA1_DIGEST_SIZE, accept, sizeof(accept)) < 0) {
        THROW("WebSocket_open: accept hash encoding failed");
        wsFail(self, "accept hash encoding failed");
        return false;
    }

    char request[WS_PATH_BYTES + 512u];
    i32 written = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        WsTarget_pathMut(target), WsTarget_hostMut(target), WsTarget_port(target), key);
    if (written < 0 || (bytes) written >= sizeof(request)) {
        THROW("WebSocket_open: handshake request does not fit");
        wsFail(self, "handshake request does not fit");
        return false;
    }
    if (!wsWrite(self, (const u8*) request, (bytes) written)) {
        THROW("WebSocket_open: handshake request write failed");
        wsFail(self, "handshake request write failed");
        return false;
    }

    // Read the response one Byte at a time: a server may send its first frame in
    // the same segment, and a bulk read would swallow those Bytes. Nothing is
    // buffered past the header, so the frame stream resumes exactly at the next
    // recv.
    char response[WS_HANDSHAKE_MAX_BYTES + 1u];
    bytes used = 0u;
    bool complete = false;
    while (used < WS_HANDSHAKE_MAX_BYTES) {
        if (!wsReadExact(self, (u8*) (response + used), 1u))
            break;
        used += 1u;
        response[used] = '\0';
        if (used >= 4u && wsFindHeaderEnd(response, used) != SIZE_MAX) {
            complete = true;
            break;
        }
    }
    response[used] = '\0';
    if (!complete) {
        THROW("WebSocket_open: handshake response incomplete");
        wsFail(self, "handshake response incomplete");
        return false;
    }
    if (strncmp(response, "HTTP/1.1 101", 12u) != 0 &&
        strncmp(response, "HTTP/1.0 101", 12u) != 0) {
        THROW("WebSocket_open: handshake rejected (expected 101)");
        wsFail(self, "handshake rejected: no 101");
        return false;
    }
    char value[256];
    if (!wsHeaderValue(response, "Upgrade", value, sizeof(value)) ||
        !wsCiEq(value, "websocket", strlen("websocket"))) {
        THROW("WebSocket_open: handshake lacks Upgrade: websocket");
        wsFail(self, "handshake lacks Upgrade");
        return false;
    }
    if (!wsHeaderValue(response, "Connection", value, sizeof(value)) ||
        !wsCiEq(value, "Upgrade", strlen("Upgrade"))) {
        THROW("WebSocket_open: handshake lacks Connection: Upgrade");
        wsFail(self, "handshake lacks Connection");
        return false;
    }
    if (!wsHeaderValue(response, "Sec-WebSocket-Accept", value, sizeof(value)) ||
        strcmp(value, accept) != 0) {
        THROW("WebSocket_open: handshake accept hash mismatch");
        wsFail(self, "handshake accept hash mismatch");
        return false;
    }
    return true;
}

// CONSTRUCTORS

/** Connect, handshake, and leave the client OPEN, or fail closed. */
bool WebSocket_open(WebSocket *self, const char *url, WebSocketConfig config) {
    if (self == nullptr || url == nullptr) {
        THROW("WebSocket_open: null argument");
        return false;
    }
    WebSocket_close(self);
    WebSocket_setTimeoutMs(self, config.timeoutMs == 0u ? 5000u : config.timeoutMs);
    (*WebSocket_maxMessageBytesMut(self)) = config.maxMessageBytes == 0u
        ? WS_MESSAGE_BYTES_DEFAULT : config.maxMessageBytes;
    WebSocket_errorMut(self)[0] = '\0';
    WebSocket_closeReasonMut(self)[0] = '\0';
    WebSocket_setCloseReasonTruncated(self, false);
    WebSocket_setCloseCode(self, 1005u);
    WebSocket_setMessageOpcode(self, 0u);
    WebSocket_setMessageLength(self, 0u);
    WebSocket_setFramesIn(self, 0u);
    WebSocket_setFramesOut(self, 0u);
    WebSocket_setPongsSent(self, 0u);

    WsTarget target;
    if (!wsParseUrl(url, &target)) {
        THROW("WebSocket_open: malformed or unsupported WebSocket URL");
        wsFail(self, "unsupported URL");
        return false;
    }
    if (target.secure) {
        TlsConn *tls = nullptr;
        if (!Tls_connect(target.host, target.port, &tls)) {
            THROW("WebSocket_open: TLS connect failed (no backend, or the peer was refused)");
            wsFail(self, "TLS connect failed");
            return false;
        }
        Tls_setBudget(tls, WebSocket_timeoutMs(self));
        WebSocket_setTls(self, tls);
    } else {
        i32 fd = wsConnect(&target, WebSocket_timeoutMs(self));
        if (fd < 0) {
            THROW("WebSocket_open: connect failed");
            wsFail(self, "connect failed");
            return false;
        }
        WebSocket_setFd(self, fd);
    }
    WebSocket_setMessage(self, (u8*) Memory_alloc(TYPE_BYTE_ARRAY, 1024u));
    if (WebSocket_message(self) == nullptr) {
        THROW("WebSocket_open: reassembly buffer allocation failed");
        wsFail(self, "reassembly buffer allocation failed");
        WebSocket_close(self);
        return false;
    }
    WebSocket_setMessageCap(self, 1024u);
    if (!wsHandshake(self, &target)) {
        WebSocket_close(self);
        return false;
    }
    WebSocket_setState(self, WEBSOCKET_OPEN);
    return true;
}

/** Send a normal close when open, drop the socket, release the owned buffer. */
fn WebSocket_close(WebSocket *self) {
    if (self == nullptr)
        return;
    if (WebSocket_state(self) == WEBSOCKET_OPEN &&
        (WebSocket_fd(self) >= 0 || WebSocket_tls(self) != nullptr))
        (discard) WebSocket_writeClose(self, 1000u, nullptr);
    if (WebSocket_tls(self) != nullptr) {
        Tls_close(WebSocket_tls(self));
        WebSocket_setTls(self, nullptr);
    }
    if (WebSocket_fd(self) >= 0) {
        close(WebSocket_fd(self));
        WebSocket_setFd(self, -1);
    }
    if (WebSocket_message(self) != nullptr) {
        Memory_free(WebSocket_message(self));
        WebSocket_setMessage(self, nullptr);
    }
    WebSocket_setMessageCap(self, 0u);
    WebSocket_setMessageLength(self, 0u);
    WebSocket_setMessageOpcode(self, 0u);
    WebSocket_setState(self, WEBSOCKET_CLOSED);
}

// CORE FUNCTIONS

/** Read frames within the budget; control frames act inline. */
WebSocketMessage WebSocket_poll(WebSocket *self, u32 budgetMs) {
    WebSocketMessage result;
    memset(&result, 0, sizeof(result));
    result.kind = WS_MESSAGE_NONE;
    result.closeCode = 1005u;
    if (self == nullptr || (WebSocket_fd(self) < 0 && WebSocket_tls(self) == nullptr)) {
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, "connection is not open", sizeof("connection is not open"));
        return result;
    }
    if (WebSocket_state(self) == WEBSOCKET_FAILED) {
        result.kind = WS_MESSAGE_ERROR;
        memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
        return result;
    }
    if (WebSocket_state(self) == WEBSOCKET_CLOSED) {
        result.kind = WS_MESSAGE_CLOSE;
        result.closeCode = WebSocket_closeCode(self);
        return result;
    }

    u64 deadline = wsNowMs() + (u64) budgetMs;
    for (;;) {
        u64 now = wsNowMs();
        if (now >= deadline)
            return result;
        u32 slice = (u32) (deadline - now);
        if (slice > 100u)
            slice = 100u; // bounded slices: re-check the budget, never park
        if (WebSocket_tls(self) != nullptr) {
            // A TLS connection has no descriptor to poll, so ask the transport
            // for readiness instead. A quiet budget keeps the connection healthy
            // and simply yields nothing this step; a broken peer is reported by
            // the read or write that actually fails.
            Tls_setBudget(WebSocket_tls(self), slice);
            if (!Tls_readable(WebSocket_tls(self), slice))
                continue;
        } else {
            struct pollfd waiting = { .fd = WebSocket_fd(self), .events = POLLIN };
            i32 ready = poll(&waiting, 1, (i32) slice);
            if (ready == 0)
                continue;
            if (ready < 0) {
                wsFail(self, "poll failed");
                result.kind = WS_MESSAGE_ERROR;
                memcpy(result.error, WebSocket_errorMut(self), sizeof(result.error));
                return result;
            }
        }
        WebSocketMessage frame = wsConsumeFrame(self);
        if (frame.kind != WS_MESSAGE_NONE)
            return frame;
    }
}

/** Send one final text frame; the payload must already be valid UTF-8. */
bool WebSocket_writeText(WebSocket *self, const char *text, bytes length) {
    if (!wsConnected(self) || text == nullptr)
        return false;
    if (!wsUtf8Valid((const u8*) text, length)) {
        // A locally rejected payload is not a transport fault: the connection
        // stays usable and the caller may send something valid next.
        THROW("WebSocket_writeText: payload is not valid UTF-8");
        return false;
    }
    if (!wsSendFrame(self, WEBSOCKET_TEXT, true, (const u8*) text, length)) {
        wsFail(self, "text frame write failed");
        return false;
    }
    return true;
}

/** Send one final binary frame. */
bool WebSocket_writeBinary(WebSocket *self, const u8 *data, bytes length) {
    if (!wsConnected(self) || (data == nullptr && length != 0u))
        return false;
    if (!wsSendFrame(self, WEBSOCKET_BINARY, true, data, length)) {
        wsFail(self, "binary frame write failed");
        return false;
    }
    return true;
}

/** Send a ping; the peer's pong is consumed silently by poll. */
bool WebSocket_writePing(WebSocket *self, const u8 *payload, bytes length) {
    if (!wsConnected(self) || (payload == nullptr && length != 0u))
        return false;
    if (length > WS_CONTROL_MAX_BYTES) {
        THROW("WebSocket_writePing: control payload exceeds 125 Bytes");
        return false;
    }
    if (!wsSendFrame(self, WEBSOCKET_PING, true, payload, length)) {
        wsFail(self, "ping write failed");
        return false;
    }
    return true;
}

/** Send a close frame and move to CLOSING; the reason is cut and flagged. */
bool WebSocket_writeClose(WebSocket *self, u16 code, const char *reason) {
    if (!wsConnected(self))
        return false;
    u8 payload[2u + WS_CLOSE_REASON_BYTES];
    bytes length = 0u;
    if (code != 0u) {
        payload[0] = (u8) (code >> 8);
        payload[1] = (u8) (code & 0xFFu);
        length = 2u;
        if (reason != nullptr) {
            bytes textLength = strlen(reason);
            bytes copy = textLength < WS_CLOSE_REASON_BYTES ? textLength : WS_CLOSE_REASON_BYTES;
            memcpy(payload + 2u, reason, copy);
            length += copy;
            WebSocket_setCloseReasonTruncated(self, copy < textLength);
        }
    }
    if (!wsSendFrame(self, WEBSOCKET_CLOSE, true, payload, length)) {
        wsFail(self, "close frame write failed");
        return false;
    }
    WebSocket_setState(self, WEBSOCKET_CLOSING);
    return true;
}

// GETTERS (the Symmetric Getter/Setter Completeness Law: null-safe)

/** The connection state, or CLOSED for a null client. */
WebSocketState WebSocket_state(const WebSocket *self) {
    return self == nullptr ? WEBSOCKET_CLOSED : WebSocket_state(self);
}

/** True only while the handshake is accepted and no close has been seen. */
bool WebSocket_isOpen(const WebSocket *self) {
    return self != nullptr && WebSocket_state(self) == WEBSOCKET_OPEN;
}

/** The bounded last-failure text, or "" when there is none. */
const char *WebSocket_lastError(const WebSocket *self) {
    return self == nullptr ? "" : WebSocket_errorMut(self);
}

/** The finalized close code, or 1005 when the peer sent none. */
u16 WebSocket_closeCode(const WebSocket *self) {
    return self == nullptr ? 1005u : WebSocket_closeCode(self);
}

/** The bounded close reason text, or "" when there is none. */
const char *WebSocket_closeReason(const WebSocket *self) {
    return self == nullptr ? "" : WebSocket_closeReasonMut(self);
}

/** The reassembly cap in force, or zero for a null client. */
bytes WebSocket_maxMessageBytes(const WebSocket *self) {
    return self == nullptr ? 0u : WebSocket_maxMessageBytes(self);
}

/** Complete frames decoded since open. */
u64 WebSocket_framesIn(const WebSocket *self) {
    return self == nullptr ? 0u : WebSocket_framesIn(self);
}

/** Complete frames encoded and sent since open. */
u64 WebSocket_framesOut(const WebSocket *self) {
    return self == nullptr ? 0u : WebSocket_framesOut(self);
}

/** Automatic pong replies sent since open. */
u64 WebSocket_pongsSent(const WebSocket *self) {
    return self == nullptr ? 0u : WebSocket_pongsSent(self);
}

// STRING PROJECTIONS (the toString Law)

/** The state's stable name for the projections. */
static const char *wsStateName(WebSocketState state) {
    switch (state) {
        case WEBSOCKET_OPEN: return "OPEN";
        case WEBSOCKET_CLOSING: return "CLOSING";
        case WEBSOCKET_FAILED: return "FAILED";
        case WEBSOCKET_CLOSED: return "CLOSED";
    }
    return "UNKNOWN";
}

/** The VALUE string: the live connection summary. */
fn WebSocket_toString(const WebSocket *self, char *dest, bytes cap, bool *outTruncated) {
    Str out = Str(dest, cap);
    if (self == nullptr) {
        Str_put(&out, "nullptr");
    } else {
        Str_printf(&out,
            "WebSocket { state=%s, fd=%d, closeCode=%u, framesIn=%llu, framesOut=%llu, "
            "pongs=%llu }",
            wsStateName(WebSocket_state(self)), WebSocket_fd(self), (u32) WebSocket_closeCode(self),
            (u64) WebSocket_framesIn(self),
            (u64) WebSocket_framesOut(self),
            (u64) WebSocket_pongsSent(self));
    }
    if (outTruncated != nullptr)
        *outTruncated = Str_isTruncated(&out);
}

/** The STRUCTURE string: this class's own fields, one layer, no recursion. */
fn WebSocket_toStringStruct(const WebSocket *self, char *dest, bytes cap,
                              bool *outTruncated) {
    Str out = Str(dest, cap);
    if (self == nullptr) {
        Str_put(&out, "nullptr");
    } else {
        Str_printf(&out,
            "WebSocket { fd=%d, state=%s, timeoutMs=%u, maxMessageBytes=%llu, "
            "message=%s, messageCap=%llu, messageLength=%llu, messageOpcode=%u, "
            "closeCode=%u, closeReasonTruncated=%s, framesIn=%llu, framesOut=%llu, "
            "pongsSent=%llu }",
            WebSocket_fd(self), wsStateName(WebSocket_state(self)), WebSocket_timeoutMs(self),
            (u64) WebSocket_maxMessageBytes(self),
            WebSocket_message(self) == nullptr ? "nullptr" : "owned",
            (u64) WebSocket_messageCap(self),
            (u64) WebSocket_messageLength(self), WebSocket_messageOpcode(self),
            (u32) WebSocket_closeCode(self),
            WebSocket_closeReasonTruncated(self) ? "true" : "false",
            (u64) WebSocket_framesIn(self),
            (u64) WebSocket_framesOut(self),
            (u64) WebSocket_pongsSent(self));
    }
    if (outTruncated != nullptr)
        *outTruncated = Str_isTruncated(&out);
}
