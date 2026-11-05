// Owner: security/chacha20poly1305.c — ChaCha20-Poly1305 AEAD (RFC 8439).
//
// Proves the section 2.8.2 vector, then attacks it: a forged tag and a flipped
// ciphertext bit must both refuse and leave no plaintext. A broken AEAD still
// returns plausible plaintext, so both the vector and the refusals are the
// evidence.
#include "security/chacha20poly1305.h"

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
    // RFC 8439 section 2.8.2.
    static const char *plain =
        "Ladies and Gentlemen of the class of '99: If I could offer you only "
        "one tip for the future, sunscreen would be it.";
    uint8_t key[32], nonce[12], aad[12];
    assert(fromHex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f",
                   key, sizeof key) == 32u);
    assert(fromHex("070000004041424344454647", nonce, sizeof nonce) == 12u);
    assert(fromHex("50515253c0c1c2c3c4c5c6c7", aad, sizeof aad) == 12u);
    static const char *cipherHex =
        "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d6"
        "3dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3"
        "692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7b"
        "c3ff4def08e4b7a9de576d26586cec64b6116";
    static const char *tagHex = "1ae10b594f09e26a7e902ecbd0600691";

    uint8_t ciphertext[128], tag[16];
    size_t plainLen = strlen(plain);
    assert(ChaCha20Poly1305_encrypt(key, nonce, aad, sizeof aad,
                                    (const uint8_t*) plain, plainLen, ciphertext, tag));
    uint8_t expected[128];
    size_t expectedLen = fromHex(cipherHex, expected, sizeof expected);
    assert(expectedLen == plainLen);
    assert(memcmp(ciphertext, expected, plainLen) == 0);
    char hex[33];
    toHex(tag, sizeof tag, hex);
    assert(strcmp(hex, tagHex) == 0);

    // Round trip.
    uint8_t recovered[128];
    assert(ChaCha20Poly1305_decrypt(key, nonce, aad, sizeof aad,
                                    ciphertext, plainLen, tag, recovered));
    assert(memcmp(recovered, plain, plainLen) == 0);

    // A forged tag refuses and leaves no plaintext.
    uint8_t forged[16];
    memcpy(forged, tag, sizeof forged);
    forged[0] ^= 0x01u;
    memset(recovered, 0xAA, sizeof recovered);
    assert(!ChaCha20Poly1305_decrypt(key, nonce, aad, sizeof aad,
                                     ciphertext, plainLen, forged, recovered));
    for (size_t i = 0; i < plainLen; ++i)
        assert(recovered[i] == 0u);

    // A flipped ciphertext bit refuses (integrity, not just the tag field).
    uint8_t tampered[128];
    memcpy(tampered, ciphertext, plainLen);
    tampered[7] ^= 0x80u;
    assert(!ChaCha20Poly1305_decrypt(key, nonce, aad, sizeof aad,
                                     tampered, plainLen, tag, recovered));

    // Changed associated data refuses: the AAD is committed to.
    uint8_t otherAad[12];
    memcpy(otherAad, aad, sizeof otherAad);
    otherAad[0] ^= 0x01u;
    assert(!ChaCha20Poly1305_decrypt(key, nonce, otherAad, sizeof otherAad,
                                     ciphertext, plainLen, tag, recovered));

    printf("chacha20poly1305_test: OK\n");
    return 0;
}
