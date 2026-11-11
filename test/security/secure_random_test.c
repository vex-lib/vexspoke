// vexspoke/test/security/secure_random_test.c — the SecureRandom class _test.
//
// An entropy source CANNOT have golden values (the Determinism and
// Reproducibility Law's explicit carve-out for nondeterministic sources), so
// this test is PROPERTY-BASED: it proves the contract's observable properties
// rather than any particular output.
//   - draw fills the whole buffer and is not all-zero (over a wide buffer);
//   - two draws differ, and two handles differ;
//   - null self / null dest-with-len are refused; len 0 is a no-op;
//   - every small/odd/word-aligned length is accepted;
//   - the telemetry counter tracks draws.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "security/secure_random.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Returns whether any byte in the supplied span is nonzero.
static bool anyNonZero(const u8 *buf, bytes len) {
    for (bytes i = 0; i < len; i++)
        if (buf[i] != 0)
            return true;
    return false;
}

// Checks SecureRandom output properties, accepted sizes, null rejection, and draw counts.
int main(void) {
    SecureRandom *sr = SecureRandom();
    CHECK(sr != nullptr);
    if (!sr) {
        printf("secure_random_test: cannot allocate handle\n");
        return 1;
    }
    SecureRandom_free(nullptr); // null-safe

    CHECK(SecureRandom_draws(sr) == 0);

    // A wide draw fills the buffer and is not all-zero (2^-256 odds otherwise).
    u8 wide[64];
    memset(wide, 0, sizeof wide);
    CHECK(SecureRandom_bytes(sr, wide, sizeof wide));
    CHECK(anyNonZero(wide, sizeof wide));

    // Refusals: null self, and null dest with a nonzero length.
    CHECK(!SecureRandom_bytes(nullptr, wide, 4));
    CHECK(!SecureRandom_bytes(sr, nullptr, 4));

    // Zero length is a successful no-op.
    CHECK(SecureRandom_bytes(sr, wide, 0));

    // Every length is accepted (word-aligned, odd, sub-word, crossing words).
    const bytes sizes[] = { 1, 2, 7, 8, 15, 16, 33, 64, 255, 256 };
    for (bytes i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        u8 buf[256];
        memset(buf, 0, sizeof buf);
        CHECK(SecureRandom_bytes(sr, buf, sizes[i]));
    }

    // Two draws differ (two independent 32-byte fills).
    u8 a[32];
    u8 b[32];
    CHECK(SecureRandom_bytes(sr, a, sizeof a));
    CHECK(SecureRandom_bytes(sr, b, sizeof b));
    CHECK(memcmp(a, b, sizeof a) != 0);

    // Two handles differ (independent streams, both OS-sourced).
    SecureRandom *sr2 = SecureRandom();
    CHECK(sr2 != nullptr);
    u8 c[32];
    u8 d[32];
    CHECK(SecureRandom_bytes(sr, c, sizeof c));
    CHECK(SecureRandom_bytes(sr2, d, sizeof d));
    CHECK(memcmp(c, d, sizeof c) != 0);

    // Scalar draws work and the telemetry counter advances.
    u64 before = SecureRandom_draws(sr);
    (void) SecureRandom_u64(sr);
    (void) SecureRandom_u32(sr);
    CHECK(SecureRandom_draws(sr) == before + 2);
    CHECK(SecureRandom_draws(nullptr) == 0);

    SecureRandom_free(sr2);
    SecureRandom_free(sr);

    if (g_failures == 0) {
        printf("secure_random_test: all property assertions held\n");
        return 0;
    }
    printf("secure_random_test: %d FAILURES\n", g_failures);
    return 1;
}
