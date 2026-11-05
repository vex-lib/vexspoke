// Owner: security/chacha20.c — ChaCha20 (RFC 8439).
//
// Proves the section 2.3.2 block-function vector and the section 2.4.2
// encryption vector, plus that the XOR is symmetric. A wrong ChaCha20 still
// emits a plausible keystream, so the vectors are the only evidence.
#include "security/chacha20.h"

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
    uint8_t key[CHACHA20_KEY_SIZE];
    for (unsigned i = 0; i < CHACHA20_KEY_SIZE; ++i)
        key[i] = (uint8_t) i;

    // RFC 8439 section 2.3.2: the block function, counter 1.
    uint8_t nonce[CHACHA20_NONCE_SIZE];
    assert(fromHex("000000090000004a00000000", nonce, sizeof nonce) == 12u);
    uint8_t block[CHACHA20_BLOCK_SIZE];
    ChaCha20_block(block, key, 1u, nonce);
    static const char *blockHex =
        "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"
        "d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e";
    char got[129];
    toHex(block, sizeof block, got);
    assert(strcmp(got, blockHex) == 0);

    // RFC 8439 section 2.4.2: encryption, counter 1.
    static const char *plain =
        "Ladies and Gentlemen of the class of '99: If I could offer you only "
        "one tip for the future, sunscreen would be it.";
    static const char *cipherHex =
        "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0b"
        "f91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d8"
        "07ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab7793736"
        "5af90bbf74a35be6b40b8eedf2785e42874d";
    uint8_t buffer[128];
    size_t plainLen = strlen(plain);
    memcpy(buffer, plain, plainLen);
    assert(fromHex("000000000000004a00000000", nonce, sizeof nonce) == 12u);
    ChaCha20_xor(buffer, plainLen, key, 1u, nonce);
    uint8_t expected[128];
    size_t expectedLen = fromHex(cipherHex, expected, sizeof expected);
    assert(expectedLen == plainLen);
    assert(memcmp(buffer, expected, plainLen) == 0);

    // XOR is its own inverse: decrypting restores the plaintext.
    ChaCha20_xor(buffer, plainLen, key, 1u, nonce);
    assert(memcmp(buffer, plain, plainLen) == 0);

    printf("chacha20_test: OK\n");
    return 0;
}
