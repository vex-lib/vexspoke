#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "security/crypto.h"
#include "security/touchid.h"

static int g_failures = 0;

#define TEST_ASSERT(expr, msg) do { \
    if (!(expr)) { \
        fprintf(stderr, "[crypto_security_test] FAIL: %s (line %d)\n", msg, __LINE__); \
        g_failures++; \
    } else { \
        printf("[crypto_security_test] PASS %s\n", msg); \
    } \
} while (0)

// Compares one-shot and chunked SHA-256 output with published NIST vectors.
static void test_sha256_nist_vectors(void) {
    printf("\n--- Testing NIST FIPS 180-4 SHA-256 Vectors ---\n");

    // Vector 1: Empty string
    char hex1[CRYPTO_SHA256_HEX_SIZE];
    Crypto_sha256Hex("", 0, hex1);
    TEST_ASSERT(strcmp(hex1, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0,
                "SHA-256 empty string matches NIST vector");

    // Vector 2: "abc"
    char hex2[CRYPTO_SHA256_HEX_SIZE];
    Crypto_sha256Hex("abc", 3, hex2);
    TEST_ASSERT(strcmp(hex2, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
                "SHA-256 'abc' matches NIST vector");

    // Vector 3: 56-byte string
    const char *v3 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    char hex3[CRYPTO_SHA256_HEX_SIZE];
    Crypto_sha256Hex(v3, strlen(v3), hex3);
    TEST_ASSERT(strcmp(hex3, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0,
                "SHA-256 56-byte message matches NIST vector");

    // Streaming API check
    CryptoSha256 ctx;
    Crypto_sha256Init(&ctx);
    Crypto_sha256Update(&ctx, "a", 1);
    Crypto_sha256Update(&ctx, "b.sh", 1);
    Crypto_sha256Update(&ctx, "c", 1);
    u8 digestStream[CRYPTO_SHA256_DIGEST_SIZE];
    Crypto_sha256Final(&ctx, digestStream);
    char hexStream[CRYPTO_SHA256_HEX_SIZE];
    Crypto_toHex(digestStream, CRYPTO_SHA256_DIGEST_SIZE, hexStream);
    TEST_ASSERT(strcmp(hexStream, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0,
                "SHA-256 streaming multi-chunk matches one-shot");
}

// Checks equality results for equal buffers and differences at both ends.
static void test_constant_time_equals(void) {
    printf("\n--- Testing Constant-Time Equality Check ---\n");
    u8 a[32] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    u8 b[32] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    u8 c[32] = { 1, 2, 3, 4, 5, 6, 7, 9 };

    TEST_ASSERT(Crypto_constantTimeEquals(a, b, 32), "identical buffers evaluate equal");
    TEST_ASSERT(!Crypto_constantTimeEquals(a, c, 32), "different last byte evaluates not equal");

    u8 d[32] = { 0, 2, 3, 4, 5, 6, 7, 8 };
    TEST_ASSERT(!Crypto_constantTimeEquals(a, d, 32), "different first byte evaluates not equal");
}

// Checks relational hash nonzero output, deterministic repeats, and distinction.
static void test_relational_hash(void) {
    printf("\n--- Testing Fast Relational Hash Mixers ---\n");
    u64 h1 = Crypto_hash64("transform.position", 18);
    u64 h2 = Crypto_hash64("transform.rotation", 18);
    u64 h3 = Crypto_hash64("transform.position", 18);

    TEST_ASSERT(h1 != 0, "hash64 produces non-zero value");
    TEST_ASSERT(h1 == h3, "hash64 is deterministic for identical input");
    TEST_ASSERT(h1 != h2, "hash64 differentiates different strings");

    u32 h32a = Crypto_hash32("camera.fov", 10);
    u32 h32b = Crypto_hash32("camera.fov", 10);
    TEST_ASSERT(h32a == h32b, "hash32 is deterministic");
}

// Checks seeded PRNG repeatability, successive values, and random-byte filling.
static void test_prng(void) {
    printf("\n--- Testing XorShift128+ PRNG Engine ---\n");
    CryptoRng rng1, rng2;
    Crypto_rngInit(&rng1, 0xCAFEBABEDEADBEEFULL);
    Crypto_rngInit(&rng2, 0xCAFEBABEDEADBEEFULL);

    u64 r1 = Crypto_rngNextU64(&rng1);
    u64 r2 = Crypto_rngNextU64(&rng2);
    TEST_ASSERT(r1 == r2, "PRNG seeded with same value generates identical first integer");

    u64 r3 = Crypto_rngNextU64(&rng1);
    TEST_ASSERT(r1 != r3, "successive PRNG integers differ");

    u8 Bytes[64];
    memset(Bytes, 0, sizeof(Bytes));
    Crypto_prngBytes(Bytes, sizeof(Bytes));
    bool hasNonZero = false;
    for (bytes i = 0; i < sizeof(Bytes); i++) {
        if (Bytes[i] != 0) {
            hasNonZero = true;
            break;
        }
    }
    TEST_ASSERT(hasNonZero, "randomBytes filled buffer with non-zero Bytes");
}

// Checks lowercase hexadecimal encoding and byte-for-byte decode round trips.
static void test_hex_conversion(void) {
    printf("\n--- Testing Hex Serialization Helpers ---\n");
    u8 raw[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    char hex[9];
    Crypto_toHex(raw, 4, hex);
    TEST_ASSERT(strcmp(hex, "deadbeef") == 0, "toHex converts correctly to lowercase hex");

    u8 decoded[4];
    bytes count = Crypto_fromHex(hex, decoded, 4);
    TEST_ASSERT(count == 4, "fromHex decoded 4 Bytes");
    TEST_ASSERT(memcmp(raw, decoded, 4) == 0, "fromHex decoded Bytes match original raw Bytes");
}

// Checks consumed/discarded TouchID tokens cannot verify successfully.
static void test_touchid_security(void) {
    printf("\n--- Testing Biometric TouchID Token Verification Protocol ---\n");
    TouchIDToken nullTok = { .magic = {0, 0}, .consumed = true };
    TEST_ASSERT(!TouchID_verify(nullTok), "consumed / null token cannot be verified");

    TouchID_discard(nullTok);
    TEST_ASSERT(!TouchID_verify(nullTok), "discarded token remains unverified");
}

// Runs crypto vectors, equality/hash/random/hex checks, and the TouchID token case.
int main(void) {
    printf("=== Running Security & Cryptography Test Suite ===\n");

    test_sha256_nist_vectors();
    test_constant_time_equals();
    test_relational_hash();
    test_prng();
    test_hex_conversion();
    test_touchid_security();

    printf("\n=== Security & Cryptography Test Summary: %d failures ===\n", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
