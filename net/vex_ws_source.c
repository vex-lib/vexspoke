#include "vex.h"
#include "net/vex_ws_source.h"

#include "lang/str.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: VexWsSource (net/vex_ws_source.c)
 * ============================================================================
 * The socket adapter behind a host's transport seam. A host registry keeps a
 * borrowed handle plus four callbacks — connect, poll, send, close — and drives
 * them with a millisecond budget, so the host owns scheduling and this class
 * owns exactly the socket and the protocol on it.
 *
 * The seam carries no payload and reports no error, which shapes the whole
 * design: the handle holds the last delivered message record so the host can
 * read what a poll delivered, and a close or failure is *recorded* rather than
 * returned as a count. poll() therefore delivers at most one complete message
 * per call and returns 1 only for text or binary. That is deliberately
 * lossless: nothing is read off the socket unless it is handed over, so a host
 * that polls more slowly than the peer sends cannot drop a message it never
 * saw. The alternative — draining the socket into a ring — would make the
 * return value a claim about frames the host can no longer find.
 *
 * The four operations are declared with the host seam's own signatures, and no
 * R3 header is included: the compatibility is structural, so the host that binds
 * them is the one place a compile-time check on the function pointers belongs.
 * That keeps one owner per seam — R2 owns the protocol and its socket, R3 owns
 * the shape, and the host owns the binding — with no reverse include in either
 * direction (the Vertical Integration Law).
 *
 * Only `ws://` is reachable because net/websocket refuses `wss://` until the TLS
 * backend behind net/tls.h reports ready; a peer that needs transport security
 * cannot be served by this adapter yet, and that is stated rather than assumed.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: VexWsSource (net/vex_ws_source.c)
 * ============================================================================
 * A live connection plus the four callbacks a host transport table binds.
 *
 * STRUCT FIELDS (Mirroring net/vex_ws_source.h):
 * ----------------------------------------------------------------------------
 *   VexWsSource {
 *     WebSocket socket;        // the embedded RFC 6455 connection
 *     char url[2048];          // owned copy of the target, re-used by connect
 *     WebSocketConfig config;  // budgets applied to every connect
 *     bool configured;         // true once open() has stored a target
 *     WebSocketMessage last;   // the most recent delivered message record
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:      VexWsSource_0() (inline, in the header)
 * Seam operations:   _connect / _poll / _send / _close (host table targets)
 * Public Core:       VexWsSource_open
 * Public Getters:    _last / _socket / _url / _isOpen
 * Public Projections:_toString / _toStringStruct
 * CONTRACT: VexWsSource() yields the empty handle; every seam operation is
 * null-safe and returns false/0 rather than dereferencing a null handle. poll()
 * delivers at most one complete message and never consumes one it does not hand
 * over, so no message is lost by polling late. last() borrows the client: the
 * payload stays valid until the next poll or close on that handle, and a close
 * or failure appears there rather than as a delivered count. send() transmits a
 * TEXT frame and refuses a payload that is not valid UTF-8 instead of silently
 * choosing another opcode. One handle is caller-serialized: the class holds no
 * lock and is not safe for concurrent callers.
 * ============================================================================
 */


;;GETTER
static inline u16 WebSocketMessage_closeCode(const WebSocketMessage *s) { return (*s).closeCode; }
;;GETTER
static inline u16 *WebSocketMessage_closeCodeMut(WebSocketMessage *s) { return &(*s).closeCode; }
;;SETTER
static inline fn WebSocketMessage_setCloseCode(WebSocketMessage *s, u16 v) { (*s).closeCode = v; }
;;GETTER
static inline char *WebSocketMessage_errorMut(WebSocketMessage *s) { return (*s).error; }
;;GETTER
static inline const char *WebSocketMessage_error(const WebSocketMessage *s) { return (*s).error; }
;;GETTER
static inline WebSocketMessageKind WebSocketMessage_kind(const WebSocketMessage *s) { return (*s).kind; }
;;GETTER
static inline WebSocketMessageKind *WebSocketMessage_kindMut(WebSocketMessage *s) { return &(*s).kind; }
;;SETTER
static inline fn WebSocketMessage_setKind(WebSocketMessage *s, WebSocketMessageKind v) { (*s).kind = v; }
;;GETTER
static inline bytes WebSocketMessage_length(const WebSocketMessage *s) { return (*s).length; }
;;GETTER
static inline bytes *WebSocketMessage_lengthMut(WebSocketMessage *s) { return &(*s).length; }
;;SETTER
static inline fn WebSocketMessage_setLength(WebSocketMessage *s, bytes v) { (*s).length = v; }
;;GETTER
static inline const u8 * WebSocketMessage_payload(const WebSocketMessage *s) { return (*s).payload; }
;;GETTER
static inline const u8 * *WebSocketMessage_payloadMut(WebSocketMessage *s) { return &(*s).payload; }
;;SETTER
static inline fn WebSocketMessage_setPayload(WebSocketMessage *s, const u8 * v) { (*s).payload = v; }

;;GETTER
static inline WebSocketConfig VexWsSource_config(const VexWsSource *s) { return (*s).config; }
;;GETTER
static inline WebSocketConfig *VexWsSource_configMut(VexWsSource *s) { return &(*s).config; }
;;SETTER
static inline fn VexWsSource_setConfig(VexWsSource *s, WebSocketConfig v) { (*s).config = v; }
;;GETTER
static inline bool VexWsSource_configured(const VexWsSource *s) { return (*s).configured; }
;;GETTER
static inline bool *VexWsSource_configuredMut(VexWsSource *s) { return &(*s).configured; }
;;SETTER
static inline fn VexWsSource_setConfigured(VexWsSource *s, bool v) { (*s).configured = v; }
;;GETTER
static inline WebSocketMessage VexWsSource_last(const VexWsSource *s) { return (*s).last; }
;;GETTER
static inline WebSocketMessage *VexWsSource_lastMut(VexWsSource *s) { return &(*s).last; }
;;SETTER
static inline fn VexWsSource_setLast(VexWsSource *s, WebSocketMessage v) { (*s).last = v; }
;;GETTER
static inline WebSocket VexWsSource_socket(const VexWsSource *s) { return (*s).socket; }
;;GETTER
static inline WebSocket *VexWsSource_socketMut(VexWsSource *s) { return &(*s).socket; }
;;SETTER
static inline fn VexWsSource_setSocket(VexWsSource *s, WebSocket v) { (*s).socket = v; }
;;GETTER
static inline char *VexWsSource_urlMut(VexWsSource *s) { return (*s).url; }
;;GETTER
static inline const char *VexWsSource_url(const VexWsSource *s) { return (*s).url; }

// CONSTRUCTORS

/** Store a target and connect, so a later _connect can retry through the table. */
bool VexWsSource_open(VexWsSource *self, const char *url, WebSocketConfig config) {
    if (self == nullptr || url == nullptr) {
        THROW("VexWsSource_open: null argument");
        return false;
    }
    bytes length = strlen(url);
    if (length + 1u > sizeof ((VexWsSource){0}).url) {
        THROW("VexWsSource_open: target URL does not fit");
        return false;
    }
    memcpy(VexWsSource_urlMut(self), url, length + 1u);
    VexWsSource_setConfig(self, config);
    VexWsSource_setConfigured(self, true);
    WebSocketMessage *last = VexWsSource_lastMut(self);
    WebSocketMessage_setKind(last, WS_MESSAGE_NONE);
    WebSocketMessage_setCloseCode(last, 1005u);
    WebSocketMessage_setPayload(last, nullptr);
    WebSocketMessage_setLength(last, 0u);
    WebSocketMessage_errorMut(last)[0] = '\0';
    return VexWsSource_connect(self);
}

// THE FOUR SEAM OPERATIONS

/** Connect (or reconnect) to the stored target; the host's retry path. */
bool VexWsSource_connect(any handle) {
    VexWsSource *self = (VexWsSource*) handle;
    if (self == nullptr || !VexWsSource_configured(self)) {
        if (self != nullptr)
            THROW("VexWsSource_connect: no target was configured");
        return false;
    }
    if (!WebSocket_open(VexWsSource_socketMut(self), VexWsSource_urlMut(self), VexWsSource_config(self))) {
        WebSocketMessage *last = VexWsSource_lastMut(self);
        WebSocketMessage_setKind(last, WS_MESSAGE_ERROR);
        WebSocketMessage_setPayload(last, nullptr);
        WebSocketMessage_setLength(last, 0u);
        bytes cap = sizeof ((WebSocketMessage){0}).error;
        const char *reason = WebSocket_lastError(VexWsSource_socketMut(self));
        bytes reasonLength = strlen(reason);
        bytes copy = reasonLength < cap - 1u ? reasonLength : cap - 1u;
        memcpy(WebSocketMessage_errorMut(last), reason, copy);
        WebSocketMessage_errorMut(last)[copy] = '\0';
        return false;
    }
    return true;
}

/** Deliver at most one complete message; never consume one we do not hand over. */
u32 VexWsSource_poll(any handle, u64 budgetMs) {
    VexWsSource *self = (VexWsSource*) handle;
    if (self == nullptr)
        return 0u;
    u32 budget = budgetMs > UINT32_MAX ? UINT32_MAX : (u32) budgetMs;
    WebSocketMessage delivered = WebSocket_poll(VexWsSource_socketMut(self), budget);
    VexWsSource_setLast(self, delivered);
    return (delivered.kind == WS_MESSAGE_TEXT ||
            delivered.kind == WS_MESSAGE_BINARY) ? 1u : 0u;
}

/** Send one TEXT frame; a non-UTF-8 payload is refused, not re-typed. */
bool VexWsSource_send(any handle, const u8 *data, u32 length) {
    VexWsSource *self = (VexWsSource*) handle;
    if (self == nullptr || (data == nullptr && length != 0u))
        return false;
    if (!WebSocket_writeText(VexWsSource_socketMut(self), (const char*) data, (bytes) length))
        return false;
    return true;
}

;;GETTER
static WebSocketMessage *VexWsSource_lastMut(VexWsSource *self) {
    return VexWsSource_lastMut(self);
}

;;GETTER
static const WebSocketConfig *VexWsSource_config(const VexWsSource *self) {
    return VexWsSource_configMut(self);
}

;;SETTER
static fn WebSocketMessage_setPayload(WebSocketMessage *message, const u8 *payload) {
    (*message).payload = payload;
}

;;SETTER
static fn WebSocketMessage_setLength(WebSocketMessage *message, bytes length) {
    (*message).length = length;
}

;;GETTER
static WebSocketMessageKind WebSocketMessage_kind(const WebSocketMessage *message) {
    return (*message).kind;
}

;;GETTER
static bytes WebSocketMessage_length(const WebSocketMessage *message) {
    return (*message).length;
}

;;GETTER
static u16 WebSocketMessage_closeCode(const WebSocketMessage *message) {
    return (*message).closeCode;
}

;;GETTER
static u32 WebSocketConfig_timeoutMs(const WebSocketConfig *config) {
    return (*config).timeoutMs;
}

;;GETTER
static bytes WebSocketConfig_maxMessageBytes(const WebSocketConfig *config) {
    return (*config).maxMessageBytes;
}

/** Close the connection and release the owned buffer. */
fn VexWsSource_close(any handle) {
    VexWsSource *self = (VexWsSource*) handle;
    if (self == nullptr)
        return;
    WebSocket_close(VexWsSource_socketMut(self));
    WebSocketMessage *last = VexWsSource_lastMut(self);
    WebSocketMessage_setPayload(last, nullptr);
    WebSocketMessage_setLength(last, 0u);
}

// GETTERS (the Symmetric Getter/Setter Completeness Law: null-safe)

/** The last delivered record, or null for a null handle. */
const WebSocketMessage *VexWsSource_last(const VexWsSource *self) {
    return self == nullptr ? nullptr : VexWsSource_lastMut(self);
}

/** The embedded client, for what the four-callback seam cannot express. */
WebSocket *VexWsSource_socket(VexWsSource *self) {
    return self == nullptr ? nullptr : VexWsSource_socketMut(self);
}

/** The stored target, or "" when none was configured. */
const char *VexWsSource_url(const VexWsSource *self) {
    return self == nullptr ? "" : VexWsSource_urlMut(self);
}

/** True only while the embedded connection is OPEN. */
bool VexWsSource_isOpen(const VexWsSource *self) {
    return self != nullptr && WebSocket_isOpen(VexWsSource_socketMut(self));
}

// STRING PROJECTIONS (the toString Law)

/** The VALUE string: the seam-facing summary. */
fn VexWsSource_toString(const VexWsSource *self, char *dest, bytes cap,
                          bool *outTruncated) {
    Str out = Str(dest, cap);
    if (self == nullptr) {
        Str_put(&out, "nullptr");
    } else {
        const WebSocketMessage *last = VexWsSource_last(self);
        Str_printf(&out, "VexWsSource { url=%s, open=%s, last=%d, closeCode=%u }",
                   VexWsSource_configured(self) ? VexWsSource_urlMut(self) : "",
                   WebSocket_isOpen(VexWsSource_socketMut(self)) ? "true" : "false",
                   (i32) WebSocketMessage_kind(last), (u32) WebSocketMessage_closeCode(last));
    }
    if (outTruncated != nullptr)
        *outTruncated = Str_isTruncated(&out);
}

/** The STRUCTURE string: this class's own fields, one layer, no recursion. */
fn VexWsSource_toStringStruct(const VexWsSource *self, char *dest, bytes cap,
                                bool *outTruncated) {
    Str out = Str(dest, cap);
    if (self == nullptr) {
        Str_put(&out, "nullptr");
    } else {
        const WebSocketConfig *config = VexWsSource_config(self);
        const WebSocketMessage *last = VexWsSource_last(self);
        Str_printf(&out,
            "VexWsSource { socket=%s, url=%s, configured=%s, timeoutMs=%u, "
            "maxMessageBytes=%llu, last=%d, lastLength=%llu, closeCode=%u }",
            WebSocket_isOpen(VexWsSource_socketMut(self)) ? "OPEN" : "not-open",
            VexWsSource_configured(self) ? VexWsSource_urlMut(self) : "",
            VexWsSource_configured(self) ? "true" : "false", WebSocketConfig_timeoutMs(config),
            (u64) WebSocketConfig_maxMessageBytes(config),
            (i32) WebSocketMessage_kind(last), (u64) WebSocketMessage_length(last),
            (u32) WebSocketMessage_closeCode(last));
    }
    if (outTruncated != nullptr)
        *outTruncated = Str_isTruncated(&out);
}
