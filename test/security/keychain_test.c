// Owner: security/keychain.c — the OS credential store seam.
//
// The read/admission paths never touch stored data, so they run every time. The
// store → load → remove round trip WRITES to the real login keychain, so it is
// opt-in: without DARKBASE_KEYCHAIN_TEST=1 this owner returns B_TEST_SKIP rather
// than silently claiming a write path it never exercised.
#define _POSIX_C_SOURCE 200809L
#include "vex.h"

#include <assert.h>
#include <stdlib.h>

#include "security/keychain.h"

#define TEST_SERVICE "com.vexgraph.darkbase.owner-test"
#define TEST_ACCOUNT "keychain-owner"

int main(void) {
    // Every refusal code has a name; unknown values do not crash.
    assert(strcmp(Keychain_codeName(KEYCHAIN_OK), "ok") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_ABSENT), "absent") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_DENIED), "denied") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_UNSUPPORTED), "unsupported") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_INVALID), "invalid") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_TOO_SMALL), "too-small") == 0);
    assert(strcmp(Keychain_codeName(KEYCHAIN_FULL), "full") == 0);
    assert(strcmp(Keychain_codeName((KeychainCode) 999), "unknown") == 0);

    // Argument admission: never reaches the OS, never dereferences a null.
    assert(Keychain_store(nullptr, "a", (const u8*) "x", 1u) == KEYCHAIN_INVALID);
    assert(Keychain_store("s", nullptr, (const u8*) "x", 1u) == KEYCHAIN_INVALID);
    assert(Keychain_store("", "a", (const u8*) "x", 1u) == KEYCHAIN_INVALID);
    assert(Keychain_store("s", "", (const u8*) "x", 1u) == KEYCHAIN_INVALID);
    assert(Keychain_store("s", "a", nullptr, 1u) == KEYCHAIN_INVALID);
    assert(Keychain_load(nullptr, "a", (u8*) "", 0u, nullptr) == KEYCHAIN_INVALID);
    assert(Keychain_load("s", "a", nullptr, 8u, nullptr) == KEYCHAIN_INVALID);
    assert(Keychain_remove(nullptr, nullptr) == KEYCHAIN_INVALID);
    u8 buf[64];
    bytes len = 123u;
    assert(Keychain_load("s", "a", buf, 0u, &len) == KEYCHAIN_INVALID);
    assert(len == 0u);

    // An overlong service is refused before the OS sees it.
    char longService[KEYCHAIN_SERVICE_MAX + 8];
    memset(longService, 's', sizeof longService);
    longService[sizeof longService - 1] = '\0';
    assert(Keychain_store(longService, "a", (const u8*) "x", 1u) == KEYCHAIN_INVALID);

    if (!Keychain_isAvailable()) {
        // No backend on this host: the contract says UNSUPPORTED, never a fallback.
        assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, buf, sizeof buf, &len) == KEYCHAIN_UNSUPPORTED);
        fprintf(stderr, "keychain_test: no backend on this host; the seam reports UNSUPPORTED\n");
        return B_TEST_SKIP;
    }

    // A read for something that was never stored is ABSENT, not an error cascade.
    assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, buf, sizeof buf, &len) == KEYCHAIN_ABSENT);
    assert(len == 0u);

    const char *optIn = getenv("DARKBASE_KEYCHAIN_TEST");
    if (optIn == nullptr || optIn[0] == '\0') {
        fprintf(stderr,
                "keychain_test: safe paths passed. NOT PROVEN: store/load/remove round trip —\n"
                "  it writes to the real login keychain, so run it deliberately:\n"
                "  DARKBASE_KEYCHAIN_TEST=1 ./tools/b.sh test keychain\n");
        return B_TEST_SKIP;
    }

    // The opt-in round trip, under a clearly-named test service it cleans up after.
    static const u8 secret[32] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    };
    assert(Keychain_store(TEST_SERVICE, TEST_ACCOUNT, secret, sizeof secret) == KEYCHAIN_OK);
    assert(Keychain_store(TEST_SERVICE, TEST_ACCOUNT, secret, sizeof secret) == KEYCHAIN_OK);  // idempotent

    u8 loaded[64];
    bytes loadedLen = 0u;
    assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, loaded, sizeof loaded, &loadedLen) == KEYCHAIN_OK);
    assert(loadedLen == sizeof secret);
    assert(memcmp(loaded, secret, sizeof secret) == 0);

    // A destination too small refuses and leaves the buffer untouched.
    u8 small[4];
    memset(small, 0x7e, sizeof small);
    bytes smallLen = 999u;
    assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, small, sizeof small, &smallLen) == KEYCHAIN_TOO_SMALL);
    assert(smallLen == 0u);
    assert(small[0] == 0x7e);

    // Replace with a different value, then remove.
    static const u8 replaced[5] = { 9, 8, 7, 6, 5 };
    assert(Keychain_store(TEST_SERVICE, TEST_ACCOUNT, replaced, sizeof replaced) == KEYCHAIN_OK);
    assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, loaded, sizeof loaded, &loadedLen) == KEYCHAIN_OK);
    assert(loadedLen == sizeof replaced && memcmp(loaded, replaced, sizeof replaced) == 0);
    assert(Keychain_remove(TEST_SERVICE, TEST_ACCOUNT) == KEYCHAIN_OK);
    assert(Keychain_remove(TEST_SERVICE, TEST_ACCOUNT) == KEYCHAIN_ABSENT);   // gone
    assert(Keychain_load(TEST_SERVICE, TEST_ACCOUNT, loaded, sizeof loaded, &loadedLen) == KEYCHAIN_ABSENT);
    printf("keychain_test: PASS (including the round trip)\n");
    return 0;
}
