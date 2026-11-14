#include "vex.h"
#include "net/tls.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Tls_curl
 * ============================================================================
 * libcurl TLS backend for non-Apple platforms. Fail-closed: the handshake is
 * not wired yet, so no Bytes are ever sent and every operation returns false
 * (the Cold-Strict, Hot-Minimal Validation Law drop-degrade). TlsConn is an
 * opaque two-word handle (backend + opaque) owned by the caller; Tls_close
 * zeroes both words. Marked ;;DRAFT until the libcurl handshake lands.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Tls_curl (net/tls_curl.c)
 * ============================================================================
 * libcurl TLS backend for non-Apple platforms.
 *
 * STRUCT FIELDS: none — procedural (operates on TLS connection handles (opaque backend))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Tls_connect(host, port, connOut)
 *   - Tls_send(conn, buf, len)
 *   - Tls_recv(conn, buf, cap, gotOut)
 *   - Tls_close(conn)
 * ============================================================================
 */


// net/tls_curl.c — libcurl TLS backend for non-Apple platforms.
;;DRAFT
;;INTENTION("libcurl https backend, fail closed until wired")

bool Tls_connect(const char *host, int port, TlsConn **connOut) {
    if (!host || !connOut)
        return false;
    if (port <= 0)
        return false;
    // Fail closed: libcurl handshake not wired yet. No Bytes are sent.
    (*connOut) = 0;
    return false;
}

bool Tls_send(TlsConn *conn, const char *buf, bytes len) {
    if (!conn || !buf)
        return false;
    if (len == 0)
        return true;
    return false;
}

bool Tls_recv(TlsConn *conn, char *buf, bytes cap, bytes *gotOut) {
    if (!conn || !buf || !gotOut)
        return false;
    if (cap == 0)
        return false;
    (*gotOut) = 0;
    return false;
}

// Nothing is ever readable from a backend that produces no connection, so the
// accessors exist only so a host compiles and links identically on every
// platform and can see for itself that this backend answered nothing.
bool Tls_readable(TlsConn *conn, u32 budgetMs) {
    (fn) conn;
    (fn) budgetMs;
    return false;
}

fn Tls_setBudget(TlsConn *conn, u32 timeoutMs) {
    (fn) conn;
    (fn) timeoutMs;
}

u32 Tls_getBudget(const TlsConn *conn) {
    (fn) conn;
    return 0u;
}

TlsBackendKind Tls_backend() {
    return TLS_BACKEND_NONE;
}

fn Tls_close(TlsConn *conn) {
    if (!conn)
        return;
    (*conn).opaque = 0;
    (*conn).backend = 0;
}
