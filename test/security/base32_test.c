// Owner: security/base32.c — RFC 4648 base32 (the TOTP setup-secret transport form).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "security/base32.h"

static void roundTrip(const char *text) {
    uint8_t bytes[64];
    size_t len = 0;
    assert(Base32_decode(text, strlen(text), bytes, sizeof bytes, &len));
    char back[128];
    bool cut = true;
    assert(Base32_encode(bytes, len, back, sizeof back, &cut));
    assert(!cut);
    assert(strcmp(back, text) == 0);
}

int main(void) {
    // RFC 4648 section 10 vectors, verbatim.
    static const struct { const char *plain, *encoded; } cases[] = {
        { "f",      "MY======" },
        { "fo",     "MZXQ====" },
        { "foo",    "MZXW6===" },
        { "foob",   "MZXW6YQ=" },
        { "fooba",  "MZXW6YTB" },
        { "foobar", "MZXW6YTBOI======" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        char out[64];
        bool cut = true;
        assert(Base32_encode((const uint8_t*) cases[i].plain, strlen(cases[i].plain),
                             out, sizeof out, &cut));
        assert(!cut);
        assert(strcmp(out, cases[i].encoded) == 0);
        roundTrip(cases[i].encoded);
    }

    // Empty input encodes to an empty string.
    char empty[8];
    bool cut = false;
    assert(Base32_encode((const uint8_t*) "", 0, empty, sizeof empty, &cut));
    assert(empty[0] == '\0' && !cut);

    // Lengths: 5 bytes -> 8 chars, and the decoder's bound.
    assert(Base32_encodedLength(0) == 0 && Base32_encodedLength(1) == 8);
    assert(Base32_encodedLength(5) == 8 && Base32_encodedLength(6) == 16);
    assert(Base32_decodedLength(8) == 5 && Base32_decodedLength(16) == 10);

    // A real 160-bit secret: 20 bytes <-> 32 characters, and back to the same bytes.
    uint8_t secret[20];
    for (size_t i = 0; i < sizeof secret; ++i)
        secret[i] = (uint8_t) (i * 7u + 1u);
    char encoded[64];
    assert(Base32_encode(secret, sizeof secret, encoded, sizeof encoded, &cut));
    assert(strlen(encoded) == 32);
    uint8_t decoded[32];
    size_t n = 0;
    assert(Base32_decode(encoded, strlen(encoded), decoded, sizeof decoded, &n));
    assert(n == sizeof secret && memcmp(decoded, secret, sizeof secret) == 0);

    // Case-insensitive, and grouping characters are tolerated (as shown to users).
    uint8_t lower[32];
    size_t ln = 0;
    char lowered[64];
    for (size_t i = 0; i < strlen(encoded); ++i)
        lowered[i] = (char) (encoded[i] >= 'A' && encoded[i] <= 'Z' ? encoded[i] + 32 : encoded[i]);
    lowered[strlen(encoded)] = '\0';
    assert(Base32_decode(lowered, strlen(lowered), lower, sizeof lower, &ln));
    assert(ln == n && memcmp(lower, decoded, n) == 0);
    assert(Base32_decode("MZXW 6YTB", 9, lower, sizeof lower, &ln));   // space ignored
    assert(ln == 5);

    // Invalid alphabet rejects; too-small destinations refuse rather than truncate.
    assert(!Base32_decode("MZXW1YTB", 8, lower, sizeof lower, &ln));   // '1' is not base32
    assert(!Base32_decode(encoded, strlen(encoded), decoded, 3, &n));
    bool truncated = false;
    assert(!Base32_encode(secret, sizeof secret, encoded, 4, &truncated));
    assert(truncated);
    // Nulls reject rather than dereference.
    assert(!Base32_encode(nullptr, 4, encoded, sizeof encoded, nullptr));
    assert(!Base32_decode(nullptr, 4, decoded, sizeof decoded, &n));
    printf("base32_test: PASS\n");
    return 0;
}
