// Owner: security/poly1305.c — Poly1305 (RFC 8439).
//
// Proves the section 2.5.2 vector and that streaming equals one-shot across a
// block boundary. A wrong Poly1305 still returns 16 plausible bytes.
#include "security/poly1305.h"

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

int main(void) {
    // RFC 8439 section 2.5.2.
    uint8_t key[POLY1305_KEY_SIZE];
    assert(fromHex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b",
                   key, sizeof key) == POLY1305_KEY_SIZE);
    static const char *message = "Cryptographic Forum Research Group";
    uint8_t tag[POLY1305_TAG_SIZE];
    assert(Poly1305_tag(key, (const uint8_t*) message, strlen(message), tag));
    static const char *expected = "a8061dc1305136c6c22b8baf0c0127a9";
    char hex[33];
    toHex(tag, sizeof tag, hex);
    assert(strcmp(hex, expected) == 0);

    // Streaming equals one-shot across a partial-block boundary.
    uint8_t message2[37];
    for (size_t i = 0; i < sizeof message2; ++i)
        message2[i] = (uint8_t) (i * 7u + 1u);
    uint8_t oneShot[POLY1305_TAG_SIZE];
    assert(Poly1305_tag(key, message2, sizeof message2, oneShot));
    Poly1305 streamed;
    Poly1305_init(&streamed, key);
    Poly1305_update(&streamed, message2, 5u);
    Poly1305_update(&streamed, message2 + 5u, 16u);
    Poly1305_update(&streamed, message2 + 21u, sizeof message2 - 21u);
    uint8_t chunked[POLY1305_TAG_SIZE];
    assert(Poly1305_final(&streamed, chunked));
    assert(memcmp(oneShot, chunked, sizeof oneShot) == 0);

    printf("poly1305_test: OK\n");
    return 0;
}
