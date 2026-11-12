// Owner: security/poly1305.c — Poly1305 (RFC 8439).
//
// Proves the section 2.5.2 vector and that streaming equals one-shot across a
// block boundary. A wrong Poly1305 still returns 16 plausible bytes.
#include "vex.h"

#include <assert.h>

#include "security/poly1305.h"

static bytes fromHex(const char *hex, u8 *out, bytes cap) {
    bytes n = 0;
    for (const char *p = hex; p[0] != '\0' && p[1] != '\0'; p += 2) {
        unsigned byte = 0;
        assert(sscanf(p, "%2x", &byte) == 1);
        assert(n < cap);
        out[n++] = (u8) byte;
    }
    return n;
}

static void toHex(const u8 *data, bytes len, char *out) {
    for (bytes i = 0; i < len; ++i)
        snprintf(out + i * 2u, 3u, "%02x", data[i]);
    out[len * 2u] = '\0';
}

int main(void) {
    // RFC 8439 section 2.5.2.
    u8 key[POLY1305_KEY_SIZE];
    assert(fromHex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b",
                   key, sizeof key) == POLY1305_KEY_SIZE);
    static const char *message = "Cryptographic Forum Research Group";
    u8 tag[POLY1305_TAG_SIZE];
    assert(Poly1305_tag(key, (const u8*) message, strlen(message), tag));
    static const char *expected = "a8061dc1305136c6c22b8baf0c0127a9";
    char hex[33];
    toHex(tag, sizeof tag, hex);
    assert(strcmp(hex, expected) == 0);

    // Streaming equals one-shot across a partial-block boundary.
    u8 message2[37];
    for (bytes i = 0; i < sizeof message2; ++i)
        message2[i] = (u8) (i * 7u + 1u);
    u8 oneShot[POLY1305_TAG_SIZE];
    assert(Poly1305_tag(key, message2, sizeof message2, oneShot));
    Poly1305 streamed;
    Poly1305_init(&streamed, key);
    Poly1305_update(&streamed, message2, 5u);
    Poly1305_update(&streamed, message2 + 5u, 16u);
    Poly1305_update(&streamed, message2 + 21u, sizeof message2 - 21u);
    u8 chunked[POLY1305_TAG_SIZE];
    assert(Poly1305_final(&streamed, chunked));
    assert(memcmp(oneShot, chunked, sizeof oneShot) == 0);

    printf("poly1305_test: OK\n");
    return 0;
}
