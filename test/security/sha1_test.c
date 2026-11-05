// Owner: security/sha1.c — SHA-1 as a streaming digest (kept for HMAC-SHA1).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "security/sha1.h"
#include "security/crypto.h"

static void hex(const uint8_t *d, size_t n, char *out) { Crypto_toHex(d, n, out); }

int main(void) {
    // FIPS 180-4 / RFC 3174 vectors.
    uint8_t d[CRYPTO_SHA1_DIGEST_SIZE];
    char h[CRYPTO_SHA1_DIGEST_SIZE * 2 + 1];

    Crypto_sha1("", 0, d);
    hex(d, sizeof d, h);
    assert(strcmp(h, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);

    Crypto_sha1("abc", 3, d);
    hex(d, sizeof d, h);
    assert(strcmp(h, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);

    // A message that crosses the 64-byte block boundary (RFC 3174 vector 3).
    const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    Crypto_sha1(m, strlen(m), d);
    hex(d, sizeof d, h);
    assert(strcmp(h, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);

    // Streaming in pieces must equal the one-shot form.
    CryptoSha1 ctx = CryptoSha1();
    Crypto_sha1Init(&ctx);
    for (size_t i = 0; i < strlen(m); ++i)
        Crypto_sha1Update(&ctx, m + i, 1);
    uint8_t streamed[CRYPTO_SHA1_DIGEST_SIZE];
    Crypto_sha1Final(&ctx, streamed);
    assert(memcmp(streamed, d, sizeof d) == 0);

    // Boundary: exactly one block, and one byte past it.
    uint8_t block[65];
    memset(block, 'a', sizeof block);
    Crypto_sha1(block, 64, d);
    Crypto_sha1(block, 65, streamed);
    assert(memcmp(d, streamed, sizeof d) != 0);
    Crypto_sha1(nullptr, 0, d);   // null data with zero length is legal
    Crypto_sha1Final(nullptr, d);            // rejected, no crash
    Crypto_sha1Init(nullptr);                // rejected, no crash
    printf("sha1_test: PASS\n");
    return 0;
}
