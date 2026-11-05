// Owner: security/argon2.c — Argon2id (RFC 9106).
//
// Proves the RFC 9106 Section 5.3 Argon2id vector exactly, plus determinism,
// salt sensitivity and parameter refusal. A wrong Argon2 still returns a
// plausible 32-byte tag, so the published vector is the only evidence that
// counts.
#include "security/argon2.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void toHex(const uint8_t *bytes, size_t len, char *out) {
    for (size_t i = 0; i < len; ++i)
        snprintf(out + i * 2u, 3u, "%02x", bytes[i]);
    out[len * 2u] = '\0';
}

int main(void) {
    // RFC 9106 Section 5.3: Argon2id, p=4, T=32, m=32, t=3, v=0x13.
    uint8_t password[32], salt[16], secret[8], associated[12];
    memset(password, 0x01, sizeof password);
    memset(salt, 0x02, sizeof salt);
    memset(secret, 0x03, sizeof secret);
    memset(associated, 0x04, sizeof associated);

    Argon2Params params = {
        .memoryKiB = 32u,
        .passes = 3u,
        .lanes = 4u,
        .tagBytes = 32u,
    };

    uint8_t tag[ARGON2_TAG_MAX];
    assert(Argon2id_derive(&params, password, sizeof password, salt, sizeof salt,
                           secret, sizeof secret, associated, sizeof associated,
                           tag, sizeof tag));
    static const char *expected =
        "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659";
    char hex[129];
    toHex(tag, 32u, hex);
    assert(strcmp(hex, expected) == 0);

    // Deterministic: same inputs, same tag.
    uint8_t again[ARGON2_TAG_MAX];
    assert(Argon2id_derive(&params, password, sizeof password, salt, sizeof salt,
                           secret, sizeof secret, associated, sizeof associated,
                           again, sizeof again));
    assert(memcmp(tag, again, 32u) == 0);

    // A changed salt changes the tag.
    uint8_t otherSalt[16];
    memset(otherSalt, 0x2a, sizeof otherSalt);
    assert(Argon2id_derive(&params, password, sizeof password, otherSalt, sizeof otherSalt,
                           secret, sizeof secret, associated, sizeof associated,
                           again, sizeof again));
    assert(memcmp(tag, again, 32u) != 0);

    // Optional inputs are truly optional.
    assert(Argon2id_derive(&params, password, sizeof password, salt, sizeof salt,
                           nullptr, 0u, nullptr, 0u, again, sizeof again));

    // Refusals: bad tag length, short salt, tiny memory, null password.
    Argon2Params bad = params;
    bad.tagBytes = 3u;
    assert(!Argon2id_derive(&bad, password, sizeof password, salt, sizeof salt,
                            nullptr, 0u, nullptr, 0u, tag, sizeof tag));
    bad = params;
    bad.memoryKiB = 8u; // m' = 4*4*floor(8/16) = 0, below 8*lanes
    assert(!Argon2id_derive(&bad, password, sizeof password, salt, sizeof salt,
                            nullptr, 0u, nullptr, 0u, tag, sizeof tag));
    assert(!Argon2id_derive(&params, password, sizeof password, salt, 4u,
                            nullptr, 0u, nullptr, 0u, tag, sizeof tag));
    assert(!Argon2id_derive(&params, nullptr, 0u, salt, sizeof salt,
                            nullptr, 0u, nullptr, 0u, tag, sizeof tag));

    printf("argon2_test: OK\n");
    return 0;
}
