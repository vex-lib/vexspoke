#ifndef NET_URL_H
#define NET_URL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// net/url.h — URI synthesis & authentication headers (Legacy:
// net/TransportProtocol.java). Everything writes into caller buffers and
// reports what fit; nothing allocates.

// Default ports per scheme ("https" => 443, else 80).
int Url_defaultPort(const char *scheme);

// Build "scheme://host[:port]/path" into _out. nullptr scheme/host/path fall
// back to "http", "", "/". Returns length written or -1 when _out is small.
int64_t Url_build(const char *scheme, const char *host, int port,
                     const char *path, char *out, bytes cap);

// RFC 4648 base64. _out needs 4*ceil(len/3)+1 Bytes. Returns length or -1.
int64_t Url_base64(const u8 *in, bytes len, char *out, bytes cap);

// "Basic base64(user:pass)" into _out. Returns length or -1.
int64_t Url_basicAuth(const char *user, const char *pass, char *out, bytes cap);

// "Bearer token" into _out. Returns length or -1.
int64_t Url_bearerAuth(const char *token, char *out, bytes cap);

// Compatibility aliases — legacy tests used NetUrl_* prefix; keep both.
#define NetUrl_defaultPort Url_defaultPort
#define NetUrl_build Url_build
#define NetUrl_base64 Url_base64
#define NetUrl_basicAuth Url_basicAuth
#define NetUrl_bearerAuth Url_bearerAuth

#endif
