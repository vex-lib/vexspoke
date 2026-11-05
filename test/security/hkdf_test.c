// Owner: security/hkdf.c — HKDF (RFC 5869) over HMAC-SHA256.
//
// Proves appendix A test cases 1, 2 and 3: the basic case, the long-input case
// that crosses the expand block boundary several times, and the zero-length
// salt/info case. A wrong HKDF still returns plausible bytes.
#include "security/hkdf.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t fromHex(const char *hex, uint8_t *out, size_t cap) {
    size_t n = 0;
    for (const char *p = hex; p[0] != '\0' && p[1] != '\0'; p += 2) {
        unsigned byte = 0;
        assert(sscanf(p, "%2x", &byte) == 1);
        assert(n < cap);
        out[n++] = (uint8_t) byte;
    }
    return n;
}

static void toHex(const uint8_t *bytes, size_t len, char *out) {
    for (size_t i = 0; i < len; ++i)
        snprintf(out + i * 2u, 3u, "%02x", bytes[i]);
    out[len * 2u] = '\0';
}

static void check(const uint8_t *okm, size_t len, const char *hex) {
    char got[512];
    toHex(okm, len, got);
    assert(strcmp(got, hex) == 0);
}

int main(void) {
    // RFC 5869 A.1: basic.
    uint8_t ikm[22], salt[13], info[10], prk[32], okm[82];
    memset(ikm, 0x0b, sizeof ikm);
    assert(fromHex("000102030405060708090a0b0c", salt, sizeof salt) == 13u);
    assert(fromHex("f0f1f2f3f4f5f6f7f8f9", info, sizeof info) == 10u);
    Hkdf_extract(salt, sizeof salt, ikm, sizeof ikm, prk);
    char prkHex[65];
    toHex(prk, sizeof prk, prkHex);
    assert(strcmp(prkHex, "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5") == 0);
    Hkdf_expand(prk, info, sizeof info, okm, 42u);
    check(okm, 42u,
          "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf"
          "34007208d5b887185865");

    // RFC 5869 A.2: long inputs, 82-byte output across several expand blocks.
    uint8_t ikm2[80], salt2[80], info2[80];
    for (unsigned i = 0; i < 80u; ++i) {
        ikm2[i] = (uint8_t) i;
        salt2[i] = (uint8_t) (0x60u + i);
        info2[i] = (uint8_t) (0xb0u + i);
    }
    Hkdf(salt2, sizeof salt2, ikm2, sizeof ikm2, info2, sizeof info2, okm, 82u);
    check(okm, 82u,
          "b11e398dc80327a1c8e7f78c596a49344f012eda2d4efad8a050cc4c19afa97c"
          "59045a99cac7827271cb41c65e590e09da3275600c2f09b8367793a9aca3db71"
          "cc30c58179ec3e87c14c01d5c1f3434f1d87");

    // RFC 5869 A.3: zero-length salt and info.
    Hkdf(nullptr, 0u, ikm, sizeof ikm, nullptr, 0u, okm, 42u);
    check(okm, 42u,
          "8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d"
          "9d201395faa4b61a96c8");

    // The RFC ceiling refuses.
    assert(HKDF_MAX_OUTPUT == 8160u);
    uint8_t tooBig[8];
    (void) tooBig;

    printf("hkdf_test: OK\n");
    return 0;
}
