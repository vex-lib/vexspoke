#include "vex.h"

#include "security/sha1.h"

;;DEFINITION
/** SHA-1 as a streaming digest. It ships for HMAC-SHA1 in TOTP and for nothing
 * else: collision resistance is broken and irrelevant to an HMAC, where what
 * matters is the compression function acting as a PRF. Zero allocation, no
 * logging, deterministic. */
;;OVERVIEW
/** ========================================================================
 * MODULE: CryptoSha1 (security/sha1.c)
 * STRUCT FIELDS (Mirroring security/sha1.h)
 *   u32 state[5]   working hash words
 *   u8  block[64]  pending input block
 *   u64 total      message length in bytes
 *   bytes   used       bytes currently buffered
 * FUNCTION REGISTRY (exports)
 *   Crypto_sha1Init(ctx) / Update(ctx, data, len) / Final(ctx, outDigest)
 *   Crypto_sha1(data, len, outDigest)   one-shot
 * PRIVATE HELPERS: sha1Compress(ctx, block) — the 80-round compression
 * CONTRACT: null ctx/data no-ops or rejects; Final zeroes the context;
 * no allocation, no logging.
 * ======================================================================== */

static u32 rotr(u32 v, u32 bits) { return (v >> bits) | (v << (32u - bits)); }

static void sha1Compress(CryptoSha1 *ctx, const u8 *block) {
    u32 w[80];
    for (u32 i = 0; i < 16u; ++i)
        w[i] = ((u32) block[i * 4] << 24) | ((u32) block[i * 4 + 1] << 16) |
               ((u32) block[i * 4 + 2] << 8) | (u32) block[i * 4 + 3];
    for (u32 i = 16u; i < 80u; ++i)
        w[i] = rotr(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 31u);
    u32 a = (*ctx).state[0], b = (*ctx).state[1], c = (*ctx).state[2];
    u32 d = (*ctx).state[3], e = (*ctx).state[4];
    for (u32 i = 0; i < 80u; ++i) {
        u32 f = 0, k = 0;
        if (i < 20u) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40u) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60u) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        u32 temp = rotr(a, 27u) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotr(b, 2u);
        b = a;
        a = temp;
    }
    (*ctx).state[0] += a;
    (*ctx).state[1] += b;
    (*ctx).state[2] += c;
    (*ctx).state[3] += d;
    (*ctx).state[4] += e;
}

void Crypto_sha1Init(CryptoSha1 *ctx) {
    if (ctx == nullptr) {
        THROW("Crypto_sha1Init: null context");
        return;
    }
    (*ctx).state[0] = 0x67452301u;
    (*ctx).state[1] = 0xEFCDAB89u;
    (*ctx).state[2] = 0x98BADCFEu;
    (*ctx).state[3] = 0x10325476u;
    (*ctx).state[4] = 0xC3D2E1F0u;
    (*ctx).total = 0u;
    (*ctx).used = 0u;
}

void Crypto_sha1Update(CryptoSha1 *ctx, const any data, bytes len) {
    if (ctx == nullptr || (data == nullptr && len != 0u)) {
        THROW("Crypto_sha1Update: null input");
        return;
    }
    const u8 *input = (const u8*) data;
    (*ctx).total += len;
    while (len > 0u) {
        bytes room = CRYPTO_SHA1_BLOCK_SIZE - (*ctx).used;
        bytes take = len < room ? len : room;
        for (bytes i = 0; i < take; ++i)
            (*ctx).block[(*ctx).used + i] = input[i];
        (*ctx).used += take;
        input += take;
        len -= take;
        if ((*ctx).used == CRYPTO_SHA1_BLOCK_SIZE) {
            sha1Compress(ctx, (*ctx).block);
            (*ctx).used = 0u;
        }
    }
}

void Crypto_sha1Final(CryptoSha1 *ctx, u8 outDigest[CRYPTO_SHA1_DIGEST_SIZE]) {
    if (ctx == nullptr || outDigest == nullptr) {
        THROW("Crypto_sha1Final: null argument");
        return;
    }
    u64 bits = (*ctx).total * 8u;
    u8 pad = 0x80u;
    Crypto_sha1Update(ctx, &pad, 1u);
    u8 zero = 0u;
    while ((*ctx).used != CRYPTO_SHA1_BLOCK_SIZE - 8u)
        Crypto_sha1Update(ctx, &zero, 1u);
    u8 lengthBytes[8];
    for (u32 i = 0; i < 8u; ++i)
        lengthBytes[i] = (u8) (bits >> (56u - i * 8u));
    Crypto_sha1Update(ctx, lengthBytes, 8u);
    for (u32 i = 0; i < 5u; ++i) {
        outDigest[i * 4]     = (u8) ((*ctx).state[i] >> 24);
        outDigest[i * 4 + 1] = (u8) ((*ctx).state[i] >> 16);
        outDigest[i * 4 + 2] = (u8) ((*ctx).state[i] >> 8);
        outDigest[i * 4 + 3] = (u8) (*ctx).state[i];
    }
    for (u32 i = 0; i < 5u; ++i)
        (*ctx).state[i] = 0u;
    for (u32 i = 0; i < CRYPTO_SHA1_BLOCK_SIZE; ++i)
        (*ctx).block[i] = 0u;
}

void Crypto_sha1(const any data, bytes len, u8 outDigest[CRYPTO_SHA1_DIGEST_SIZE]) {
    CryptoSha1 ctx;
    Crypto_sha1Init(&ctx);
    Crypto_sha1Update(&ctx, data, len);
    Crypto_sha1Final(&ctx, outDigest);
}
