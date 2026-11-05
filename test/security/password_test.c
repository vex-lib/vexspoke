// Owner: security/password.c — PBKDF2, the slow password hash.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "security/password.h"
#include "security/hmac.h"
#include "security/crypto.h"

int main(void) {
    // RFC 6070 PBKDF2-HMAC-SHA1 vectors, verbatim.
    uint8_t key[32];
    char hex[65];
    const uint8_t *salt = (const uint8_t*) "salt";

    assert(Password_deriveSha1("password", salt, 4, 1u, key, 20u));
    Crypto_toHex(key, 20u, hex);
    assert(strcmp(hex, "0c60c80f961f0e71f3a9b524af6012062fe037a6") == 0);

    assert(Password_deriveSha1("password", salt, 4, 2u, key, 20u));
    Crypto_toHex(key, 20u, hex);
    assert(strcmp(hex, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957") == 0);

    assert(Password_deriveSha1("password", salt, 4, 4096u, key, 20u));
    Crypto_toHex(key, 20u, hex);
    assert(strcmp(hex, "4b007901b765489abead49d926f721d065a429c1") == 0);

    // Multi-block output (25 bytes) and a 36-byte salt — RFC 6070's last vector.
    const char *longSalt = "saltSALTsaltSALTsaltSALTsaltSALTsalt";
    assert(Password_deriveSha1("passwordPASSWORDpassword", (const uint8_t*) longSalt,
                               strlen(longSalt), 4096u, key, 25u));
    Crypto_toHex(key, 25u, hex);
    assert(strcmp(hex, "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038") == 0);

    // SHA-256 at one iteration is by definition HMAC(P, salt || 0x00000001).
    uint8_t message[8] = { 's', 'a', 'l', 't', 0, 0, 0, 1 };
    uint8_t expected[32], derived[32];
    Hmac_sha256((const uint8_t*) "password", 8u, message, sizeof message, expected);
    assert(Password_deriveSha256("password", salt, 4, 1u, derived, 32u));
    assert(memcmp(derived, expected, 32u) == 0);

    // Determinism and salt sensitivity.
    uint8_t again[32];
    assert(Password_deriveSha256("password", salt, 4, 1000u, derived, 32u));
    assert(Password_deriveSha256("password", salt, 4, 1000u, again, 32u));
    assert(memcmp(derived, again, 32u) == 0);
    assert(Password_deriveSha256("password", (const uint8_t*) "salu", 4, 1000u, again, 32u));
    assert(memcmp(derived, again, 32u) != 0);

    // Verify accepts the right password and refuses the wrong one.
    assert(Password_verifySha256("password", salt, 4, 1000u, derived, 32u));
    assert(!Password_verifySha256("passwore", salt, 4, 1000u, derived, 32u));

    // A fresh salt comes from the OS CSPRNG: right length, not constant.
    uint8_t s1[PASSWORD_SALT_MIN], s2[PASSWORD_SALT_MIN];
    assert(Password_generateSalt(s1, sizeof s1));
    assert(Password_generateSalt(s2, sizeof s2));
    assert(memcmp(s1, s2, sizeof s1) != 0);

    // Bounds reject loudly rather than silently weakening the derivation.
    assert(!Password_deriveSha256("password", salt, 4, 0u, derived, 32u));      // no iterations
    assert(!Password_deriveSha256("password", salt, 0u, 1000u, derived, 32u));    // empty salt
    uint8_t huge[80];
    memset(huge, 1, sizeof huge);
    assert(!Password_deriveSha256("password", huge, sizeof huge, 1000u, derived, 32u));
    assert(!Password_deriveSha256(nullptr, salt, 4, 1000u, derived, 32u));
    assert(!Password_deriveSha256("password", salt, 4, 1000u, nullptr, 32u));
    assert(!Password_generateSalt(s1, 4u));
    assert(!Password_verifySha256("password", salt, 4, 1000u, nullptr, 32u));
    printf("password_test: PASS\n");
    return 0;
}
