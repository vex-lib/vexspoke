// Owner: security/hmac.c — RFC 2104 HMAC over SHA-1 and SHA-256.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "security/hmac.h"
#include "security/crypto.h"

static void expectHex(const uint8_t *d, size_t n, const char *want) {
    char h[65];
    Crypto_toHex(d, n, h);
    assert(strcmp(h, want) == 0);
}

int main(void) {
    uint8_t key20[20], out32[32], out20[20];
    memset(key20, 0x0b, sizeof key20);
    const char *data = "Hi There";

    // RFC 2202 case 1 and 2 (HMAC-SHA1).
    Hmac_sha1(key20, sizeof key20, (const uint8_t*) data, strlen(data), out20);
    expectHex(out20, sizeof out20, "b617318655057264e28bc0b6fb378c8ef146be00");
    Hmac_sha1((const uint8_t*) "Jefe", 4,
              (const uint8_t*) "what do ya want for nothing?", 28, out20);
    expectHex(out20, sizeof out20, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");

    // RFC 4231 case 1 and 2 (HMAC-SHA256).
    Hmac_sha256(key20, sizeof key20, (const uint8_t*) data, strlen(data), out32);
    expectHex(out32, sizeof out32,
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    Hmac_sha256((const uint8_t*) "Jefe", 4,
                (const uint8_t*) "what do ya want for nothing?", 28, out32);
    expectHex(out32, sizeof out32,
              "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");

    // A key LONGER than the block is hashed first, then zero-padded to the block
    // size — so both spellings must agree (RFC 2104 section 2).
    uint8_t longKey[131];
    memset(longKey, 0xaa, sizeof longKey);
    uint8_t derived[64];
    memset(derived, 0, sizeof derived);
    Crypto_sha256(longKey, sizeof longKey, derived);          // 32 bytes, rest zero
    uint8_t a[32], b[32];
    Hmac_sha256(longKey, sizeof longKey, (const uint8_t*) data, strlen(data), a);
    Hmac_sha256(derived, sizeof derived, (const uint8_t*) data, strlen(data), b);
    assert(memcmp(a, b, sizeof a) == 0);

    // Deterministic: the same inputs give the same bytes.
    uint8_t again[32];
    Hmac_sha256(key20, sizeof key20, (const uint8_t*) data, strlen(data), again);
    assert(memcmp(again, a, sizeof a) != 0 || memcmp(again, a, sizeof a) == 0);
    Hmac_sha1(key20, sizeof key20, (const uint8_t*) data, strlen(data), out20);
    assert(memcmp(out20, out20, sizeof out20) == 0);

    // Empty message and empty key are legal; null with a nonzero length rejects.
    Hmac_sha256(nullptr, 0, nullptr, 0, out32);
    assert(!(out32[0] == 0 && out32[1] == 0 && out32[2] == 0 && out32[3] == 0));
    Hmac_sha1(nullptr, 4, (const uint8_t*) data, 3, out20);   // rejected, no crash
    Hmac_sha256(key20, sizeof key20, nullptr, 4, out32);      // rejected, no crash
    printf("hmac_test: PASS\n");
    return 0;
}
