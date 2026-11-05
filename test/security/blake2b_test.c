// Owner: security/blake2b.c — BLAKE2b (RFC 7693).
//
// Proves the implementation against the RFC 7693 vector and the official BLAKE2
// known-answer tests (unkeyed empty, keyed empty), and proves streaming equals
// one-shot across block boundaries. A wrong BLAKE2b still returns 64 plausible
// bytes, so only the published vectors count as evidence.
#include "security/blake2b.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void toHex(const uint8_t *bytes, size_t len, char *out) {
    for (size_t i = 0; i < len; ++i)
        snprintf(out + i * 2u, 3u, "%02x", bytes[i]);
    out[len * 2u] = '\0';
}

static void checkDigest(const uint8_t *bytes, size_t len, const char *hex) {
    char got[129];
    toHex(bytes, len, got);
    assert(strcmp(got, hex) == 0);
}

int main(void) {
    uint8_t out[BLAKE2B_DIGEST_MAX];

    // RFC 7693 Appendix A: BLAKE2b-512("abc").
    static const char *abc =
        "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
        "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923";
    assert(Blake2b_digest("abc", 3u, BLAKE2B_DIGEST_DEFAULT, out, sizeof out));
    checkDigest(out, BLAKE2B_DIGEST_DEFAULT, abc);

    // Official BLAKE2 KAT: unkeyed empty message.
    static const char *empty =
        "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419"
        "d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce";
    assert(Blake2b_digest("", 0u, BLAKE2B_DIGEST_DEFAULT, out, sizeof out));
    checkDigest(out, BLAKE2B_DIGEST_DEFAULT, empty);

    // Official BLAKE2 KAT: keyed empty, key = 00 01 .. 3f, digest 64.
    uint8_t key[BLAKE2B_KEY_MAX];
    for (unsigned i = 0; i < BLAKE2B_KEY_MAX; ++i)
        key[i] = (uint8_t) i;
    static const char *keyedEmpty =
        "10ebb67700b1868efb4417987acf4690ae9d972fb7a590c2f02871799aaa4786"
        "b5e996e8f0f4eb981fc214b005f42d2ff4233499391653df7aefcbc13fc51568";
    Blake2b keyed = Blake2b_0();
    assert(Blake2b_initKeyed(&keyed, BLAKE2B_DIGEST_DEFAULT, key, sizeof key));
    assert(Blake2b_final(&keyed, out, sizeof out));
    checkDigest(out, BLAKE2B_DIGEST_DEFAULT, keyedEmpty);

    // Streaming equals one-shot across a block boundary and a partial tail.
    uint8_t message[1000];
    for (size_t i = 0; i < sizeof message; ++i)
        message[i] = (uint8_t) (i * 31u + 7u);
    uint8_t oneShot[BLAKE2B_DIGEST_MAX];
    assert(Blake2b_digest(message, sizeof message, 32u, oneShot, sizeof oneShot));
    Blake2b streamed = Blake2b_0();
    assert(Blake2b_init(&streamed, 32u));
    Blake2b_update(&streamed, message, 1u);
    Blake2b_update(&streamed, message + 1u, 127u);
    Blake2b_update(&streamed, message + 128u, 128u);
    Blake2b_update(&streamed, message + 256u, sizeof message - 256u);
    uint8_t chunked[BLAKE2B_DIGEST_MAX];
    assert(Blake2b_final(&streamed, chunked, sizeof chunked));
    assert(memcmp(oneShot, chunked, 32u) == 0);

    // A truncated digest is a different, shorter vector, not a prefix guarantee.
    assert(Blake2b_digest("abc", 3u, 32u, out, sizeof out));
    assert(Blake2b_digest("abc", 3u, 64u, chunked, sizeof chunked));
    assert(memcmp(out, chunked, 32u) != 0);

    // Bounds: short destination and bad lengths reject.
    assert(!Blake2b_digest("abc", 3u, 64u, out, 32u));
    assert(!Blake2b_digest("abc", 3u, 0u, out, sizeof out));
    assert(!Blake2b_digest("abc", 3u, 65u, out, sizeof out));

    printf("blake2b_test: OK\n");
    return 0;
}
