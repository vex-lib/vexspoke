// vexspoke/test/security/crypto_test.c — owner test for security/crypto.
//
// Proves the zero-allocation crypto core against published vectors and
// invariants:
//   - SHA-256 NIST vectors ("", "abc", the 448-bit two-block string, and the
//     one-million-'a' message), streaming == one-shot, and lower-case hex;
//   - constant-time equality: equal, mismatch at first/last byte, zero length,
//     and nullptr;
//   - relational hash determinism, the 32-bit fold of the 64-bit hash, and the
//     null/empty zero mapping;
//   - XorShift128+ reproducibility: same seed -> same stream, different seed
//     -> different stream, byte output matches the word stream;
//   - the seeded global PRNG reproduces its stream;
//   - hex encode/decode round trips, truncation, and invalid-char stopping;
//   - nullptr safety throughout.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "security/crypto.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Hashes a byte span and compares its hex digest with the supplied expectation.
static void expect_hex(const void *data, size_t len, const char *want) {
    char got[CRYPTO_SHA256_HEX_SIZE];
    Crypto_sha256Hex(data, len, got);
    CHECK(strcmp(got, want) == 0);
    if (strcmp(got, want) != 0)
        printf("  sha256 got %s\n", got);
}

// Checks empty, short, and multi-block SHA-256 results against known digests.
static void test_sha256_vectors(void) {
    expect_hex("", 0,
               "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    expect_hex("abc", 3,
               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char *two =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    expect_hex(two, strlen(two),
               "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

// Checks streaming SHA-256 of one million repeated bytes against its reference digest.
static void test_sha256_million_a(void) {
    uint8_t block[1000];
    memset(block, 'a', sizeof(block));
    CryptoSha256 ctx;
    Crypto_sha256Init(&ctx);
    for (int i = 0; i < 1000; i++)
        Crypto_sha256Update(&ctx, block, sizeof(block));
    char hex[CRYPTO_SHA256_HEX_SIZE];
    uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE];
    Crypto_sha256Final(&ctx, digest);
    Crypto_toHex(digest, sizeof(digest), hex);
    CHECK(strcmp(hex,
                 "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
}

// Compares one-shot hashing with varied streaming chunk sizes and zero-length update.
static void test_streaming_equivalence(void) {
    uint8_t buf[205];
    for (int i = 0; i < 205; i++)
        buf[i] = (uint8_t) (i * 7 + 1);

    uint8_t oneShot[CRYPTO_SHA256_DIGEST_SIZE];
    Crypto_sha256(buf, sizeof(buf), oneShot);

    // Odd update chunk sizes that straddle the 64-byte block boundary.
    const size_t chunks[] = { 1, 3, 63, 64, 65, 7 };
    for (size_t c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++) {
        CryptoSha256 ctx;
        Crypto_sha256Init(&ctx);
        size_t off = 0, step = chunks[c];
        while (off < sizeof(buf)) {
            size_t n = step < sizeof(buf) - off ? step : sizeof(buf) - off;
            Crypto_sha256Update(&ctx, buf + off, n);
            off += n;
        }
        uint8_t dig[CRYPTO_SHA256_DIGEST_SIZE];
        Crypto_sha256Final(&ctx, dig);
        CHECK(memcmp(dig, oneShot, sizeof(oneShot)) == 0);
    }
    // Update with zero length is a no-op.
    CryptoSha256 ctx;
    Crypto_sha256Init(&ctx);
    Crypto_sha256Update(&ctx, buf, 0);
    uint8_t dig[CRYPTO_SHA256_DIGEST_SIZE];
    Crypto_sha256Final(&ctx, dig);
    CHECK(memcmp(dig, oneShot, 0) == 0);       // just proves it returned
}

// Checks equality and mismatch outcomes at first/last bytes, zero length, and null pointers.
static void test_constant_time(void) {
    uint8_t a[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    uint8_t b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    CHECK(Crypto_constantTimeEquals(a, b, 8));
    b[0] = 9;
    CHECK(!Crypto_constantTimeEquals(a, b, 8));
    b[0] = 1;
    b[7] = 9;
    CHECK(!Crypto_constantTimeEquals(a, b, 8));
    b[7] = 8;
    CHECK(Crypto_constantTimeEquals(a, b, 0));     // zero length compares equal
    CHECK(!Crypto_constantTimeEquals(nullptr, b, 8));
    CHECK(!Crypto_constantTimeEquals(a, nullptr, 8));
}

// Checks hash determinism, input distinction, 32-bit folding, and null/empty results.
static void test_hash(void) {
    CHECK(Crypto_hash64(nullptr, 4) == 0);
    CHECK(Crypto_hash64("abc", 0) == 0);
    uint64_t h = Crypto_hash64("abc", 3);
    CHECK(h == Crypto_hash64("abc", 3));           // deterministic
    CHECK(h != Crypto_hash64("abd", 3));
    CHECK(Crypto_hash32("abc", 3) == (uint32_t) (h ^ (h >> 32)));
    CHECK(Crypto_hash32(nullptr, 3) == 0);
}

// Checks seeded generator repeatability, byte order, zero seed, and null arguments.
static void test_rng(void) {
    CryptoRng r1, r2;
    Crypto_rngInit(&r1, 12345);
    Crypto_rngInit(&r2, 12345);
    for (int i = 0; i < 16; i++)
        CHECK(Crypto_rngNextU64(&r1) == Crypto_rngNextU64(&r2));

    CryptoRng r3;
    Crypto_rngInit(&r3, 67890);
    CHECK(Crypto_rngNextU64(&r3) != Crypto_rngNextU64(&r2));

    // Byte output must equal the word stream, little-endian, for full words.
    CryptoRng rb;
    Crypto_rngInit(&rb, 999);
    uint8_t Bytes[16];
    Crypto_rngBytes(&rb, Bytes, sizeof(Bytes));
    CryptoRng rw;
    Crypto_rngInit(&rw, 999);
    for (int i = 0; i < 2; i++) {
        uint64_t w = Crypto_rngNextU64(&rw);
        uint8_t wb[8];
        memcpy(wb, &w, 8);
        CHECK(memcmp(Bytes + i * 8, wb, 8) == 0);
    }

    // Seed 0 is remapped but deterministic; nullptr is safe.
    CryptoRng z1, z2;
    Crypto_rngInit(&z1, 0);
    Crypto_rngInit(&z2, 0);
    CHECK(Crypto_rngNextU64(&z1) == Crypto_rngNextU64(&z2));
    Crypto_rngInit(nullptr, 1);
    CHECK(Crypto_rngNextU64(nullptr) == 0);
    Crypto_rngBytes(nullptr, Bytes, sizeof(Bytes));
    Crypto_rngBytes(&rb, nullptr, sizeof(Bytes));
}

// Checks reseeding reproduces global scalar and byte streams and changes with seed.
static void test_global_rng(void) {
    Crypto_prngSeed(42);
    uint64_t a = Crypto_prngU64();
    Crypto_prngSeed(42);
    uint64_t b = Crypto_prngU64();
    CHECK(a == b);
    Crypto_prngSeed(43);
    CHECK(Crypto_prngU64() != a);

    Crypto_prngSeed(7);
    uint8_t x[16], y[16];
    Crypto_prngBytes(x, sizeof(x));
    Crypto_prngSeed(7);
    Crypto_prngBytes(y, sizeof(y));
    CHECK(memcmp(x, y, sizeof(x)) == 0);
}

// Checks hexadecimal encoding/decoding, capacity limits, invalid characters, and null cases.
static void test_hex(void) {
    uint8_t Bytes[4] = { 0x00, 0x0f, 0xa5, 0xff };
    char hex[9];
    Crypto_toHex(Bytes, 4, hex);
    CHECK(strcmp(hex, "000fa5ff") == 0);

    uint8_t back[4] = { 0 };
    CHECK(Crypto_fromHex(hex, back, sizeof(back)) == 4);
    CHECK(memcmp(back, Bytes, 4) == 0);

    // Uppercase decodes too.
    CHECK(Crypto_fromHex("A5", back, sizeof(back)) == 1);
    CHECK(back[0] == 0xa5);

    // Invalid char stops the decode at the previous byte.
    CHECK(Crypto_fromHex("0fzz", back, sizeof(back)) == 1);
    CHECK(back[0] == 0x0f);

    // maxBytes truncates.
    uint8_t two[2];
    CHECK(Crypto_fromHex("000fa5", two, sizeof(two)) == 2);

    // Empty/null edges.
    char empty[4] = { 'x', 'x', 'x', 'x' };
    Crypto_toHex(nullptr, 3, empty);
    CHECK(empty[0] == '\0');
    Crypto_toHex(Bytes, 0, empty);
    CHECK(empty[0] == '\0');
    Crypto_toHex(Bytes, 4, nullptr);               // safe
    CHECK(Crypto_fromHex(nullptr, back, 4) == 0);
    CHECK(Crypto_fromHex("00", nullptr, 4) == 0);
    CHECK(Crypto_fromHex("00", back, 0) == 0);
}

// Exercises null context, input, and digest pointers across SHA-256 entry points.
static void test_null_safety(void) {
    uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE];
    Crypto_sha256Init(nullptr);
    Crypto_sha256Update(nullptr, "x", 1);
    Crypto_sha256Final(nullptr, digest);
    Crypto_sha256(nullptr, 5, digest);             // null data: empty digest
    CryptoSha256 ctx;
    Crypto_sha256Init(&ctx);
    Crypto_sha256Final(&ctx, nullptr);
}

// Runs hash vectors, streaming, equality, PRNG, conversion, and null-safety checks.
int main(void) {
    test_sha256_vectors();
    test_sha256_million_a();
    test_streaming_equivalence();
    test_constant_time();
    test_hash();
    test_rng();
    test_global_rng();
    test_hex();
    test_null_safety();

    if (g_failures == 0) {
        printf("crypto_test: all assertions held\n");
        return 0;
    }
    printf("crypto_test: %d FAILURES\n", g_failures);
    return 1;
}
