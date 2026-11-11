#ifndef SECURITY_BLAKE2B_H
#define SECURITY_BLAKE2B_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

// security/blake2b.h — BLAKE2b (RFC 7693), the hash Argon2 is built on.
//
// BLAKE2b is a fast cryptographic hash, unkeyed or keyed. It is here for one
// reason above all: RFC 9106 builds Argon2 from BLAKE2b, so a self-contained
// Argon2id needs a self-contained BLAKE2b first. Keyed BLAKE2b is also a fine
// MAC, but HMAC-SHA256 stays the interoperable one for TOTP and webhooks.
//
// Streaming, zero-allocation, deterministic. The digest length is fixed at init
// (1..64 bytes) and the key, when present, is 1..64 bytes. No heap, no logging.
// Correctness is proved by the RFC 7693 vector and the official BLAKE2 KATs in
// tests/vexspoke/security/blake2b_test.c, not by "it produced 64 bytes".

#define BLAKE2B_BLOCK_SIZE      128u
#define BLAKE2B_DIGEST_MIN        1u
#define BLAKE2B_DIGEST_MAX       64u
#define BLAKE2B_DIGEST_DEFAULT   64u
#define BLAKE2B_KEY_MIN           1u
#define BLAKE2B_KEY_MAX          64u

typedef struct Blake2b {
    u64 h[8];                     // working hash words
    u64 t[2];                     // 128-bit byte counter
    u8  buf[BLAKE2B_BLOCK_SIZE];  // pending block
    bytes   used;                     // bytes buffered
    bytes   outlen;                   // digest length chosen at init
} Blake2b;

static inline Blake2b Blake2b_0(void) { return (Blake2b) {0}; }
#define Blake2b(...) CONSTRUCTOR_DISPATCH(Blake2b, __VA_ARGS__)

// Init unkeyed. outlen in [1,64]. False (with THROW) on a null self or bad outlen.
bool Blake2b_init(Blake2b *self, bytes outlen);
// Init keyed (a MAC). keylen 0 is the same as init; key may be null only then.
bool Blake2b_initKeyed(Blake2b *self, bytes outlen, const u8 *key, bytes keylen);

// Absorb `len` bytes. A null self is a no-op; null data with a nonzero len rejects.
void Blake2b_update(Blake2b *self, const void *data, bytes len);
// Write (*self).outlen bytes into out. False (with THROW) when out is null or
// outCap is shorter than outlen. Zeroes the context on success.
bool Blake2b_final(Blake2b *self, u8 *out, bytes outCap);

// One-shot unkeyed digest of `data` (outCap must be >= outlen).
bool Blake2b_digest(const void *data, bytes len, bytes outlen, u8 *out, bytes outCap);

#endif
