#include "security/password.h"
#include "security/argon2.h"
#include <string.h>
#include "security/hmac.h"
#include "security/crypto.h"
#include "security/secure_random.h"
#include "util/throw.h"
#include "util/annotation.h"

;;DEFINITION
/** Password owns the cold password-verifier boundary. Its legacy raw path is
 * PBKDF2 as RFC 8018 defines it, over the HMACs this module already owns; its
 * versioned record can also dispatch to Argon2id as RFC 9106 defines it. The
 * whole purpose is to be slow: the work factor is the thing standing between a
 * stolen file and a re-used password. Salt length is bounded on the PBKDF2 path
 * so that an over-long salt is refused rather than truncated, because a
 * silently shortened salt is a silently weaker hash. */
;;OVERVIEW
/** ========================================================================
 * MODULE: Password (security/password.c)
 * FUNCTION REGISTRY (exports)
 *   Password_deriveSha1(password, salt, saltLen, iterations, outKey, keyLen)
 *   Password_deriveSha256(..same..)
 *   Password_verifySha256(password, salt, saltLen, iterations, expected, expectedLen)
 *   Password_generateSalt(dest, len)      OS CSPRNG
 *   Password_createVerifier(kdf, password) versioned KDF record
 *   PasswordVerifier_isValid(self)         cold record validation
 *   PasswordVerifier_verify(self, password) versioned verification
 * PRIVATE HELPERS:
 *   derive(prf, digestSize, password, salt, saltLen, iterations, outKey, keyLen)
 * CONTRACT: null/empty input, an out-of-range salt, zero iterations or a zero key
 * reject through THROW with the output untouched; no allocation; no logging.
 * ======================================================================== */

typedef void (*PasswordPrf)(const u8 *key, bytes keyLen, const u8 *data,
                            bytes dataLen, u8 *out);

static bool derive(PasswordPrf prf, bytes digestSize, const char *password, bytes passwordLen,
                   const u8 *salt, bytes saltLen, u32 iterations,
                   u8 *outKey, bytes keyLen) {
    if (prf == nullptr || password == nullptr || passwordLen == 0u || salt == nullptr ||
        outKey == nullptr || keyLen == 0u || iterations == 0u) {
        THROW("Password_derive: invalid input");
        return false;
    }
    if (saltLen == 0u || saltLen > PASSWORD_SALT_MAX) {
        // The ALGORITHM accepts any salt; the POLICY minimum applies when a salt is
        // generated (Password_generateSalt). Refusing a short salt here would make
        // PBKDF2 untestable against its own published vectors.
        THROW("Password_derive: salt length out of range");
        return false;
    }
    u8 message[PASSWORD_SALT_MAX + 4u];
    u8 u[32];
    u8 t[32];
    for (bytes i = 0; i < saltLen; ++i)
        message[i] = salt[i];
    bytes done = 0u;
    u32 block = 1u;
    while (done < keyLen) {
        message[saltLen]     = (u8) (block >> 24);
        message[saltLen + 1] = (u8) (block >> 16);
        message[saltLen + 2] = (u8) (block >> 8);
        message[saltLen + 3] = (u8) block;
        prf((const u8*) password, passwordLen, message, saltLen + 4u, u);
        for (bytes i = 0; i < digestSize; ++i)
            t[i] = u[i];
        for (u32 round = 1u; round < iterations; ++round) {
            prf((const u8*) password, passwordLen, u, digestSize, u);
            for (bytes i = 0; i < digestSize; ++i)
                t[i] ^= u[i];
        }
        bytes take = keyLen - done < digestSize ? keyLen - done : digestSize;
        for (bytes i = 0; i < take; ++i)
            outKey[done + i] = t[i];
        done += take;
        ++block;
    }
    for (bytes i = 0; i < sizeof message; ++i)
        message[i] = 0u;
    for (bytes i = 0; i < sizeof u; ++i) {
        u[i] = 0u;
        t[i] = 0u;
    }
    return true;
}

bool Password_deriveSha1(const char *password, const u8 *salt, bytes saltLen,
                         u32 iterations, u8 *outKey, bytes keyLen) {
    bytes len = password == nullptr ? 0u : __builtin_strlen(password);
    return derive(Hmac_sha1, HMAC_SHA1_DIGEST_SIZE, password, len, salt, saltLen, iterations, outKey, keyLen);
}

bool Password_deriveSha256(const char *password, const u8 *salt, bytes saltLen,
                           u32 iterations, u8 *outKey, bytes keyLen) {
    bytes len = password == nullptr ? 0u : __builtin_strlen(password);
    return derive(Hmac_sha256, HMAC_SHA256_DIGEST_SIZE, password, len, salt, saltLen, iterations, outKey, keyLen);
}

bool Password_verifySha256(const char *password, const u8 *salt, bytes saltLen,
                           u32 iterations, const u8 *expected, bytes expectedLen) {
    if (expected == nullptr || expectedLen == 0u || expectedLen > 32u) {
        THROW("Password_verifySha256: invalid stored key");
        return false;
    }
    u8 derived[32];
    if (!Password_deriveSha256(password, salt, saltLen, iterations, derived, expectedLen))
        return false;
    bool same = Crypto_constantTimeEquals(derived, expected, expectedLen);
    for (bytes i = 0; i < sizeof derived; ++i)
        derived[i] = 0u;
    return same;
}

bool Password_generateSalt(u8 *dest, bytes len) {
    if (dest == nullptr || len < PASSWORD_SALT_MIN || len > PASSWORD_SALT_MAX) {
        THROW("Password_generateSalt: invalid length (use the OS CSPRNG, not Crypto_prng*)");
        return false;
    }
    SecureRandom *rng = SecureRandom_0();
    if (rng == nullptr)
        return false;
    bool ok = SecureRandom_bytes(rng, dest, len);
    SecureRandom_free(rng);
    return ok;
}

PasswordVerifier Password_createVerifier(PasswordKdf kdf, const char *password) {
    PasswordVerifier verifier = {0};
    verifier.code = PASSWORD_CODE_INVALID;
    if (password == nullptr || password[0] == '\0') {
        THROW("Password_createVerifier: empty password");
        return verifier;
    }
    if (kdf != PASSWORD_KDF_PBKDF2_SHA256 && kdf != PASSWORD_KDF_ARGON2ID) {
        THROW("Password_createVerifier: unsupported KDF");
        verifier.code = PASSWORD_CODE_UNSUPPORTED_KDF;
        return verifier;
    }
    verifier.kdf = (u32) kdf;
    verifier.saltLength = PASSWORD_SALT_MIN;
    if (!Password_generateSalt(verifier.salt, verifier.saltLength)) {
        verifier.code = PASSWORD_CODE_DERIVATION_FAILED;
        return verifier;
    }
    bytes passwordLen = __builtin_strlen(password);
    bool ok = false;
    if (kdf == PASSWORD_KDF_PBKDF2_SHA256) {
        verifier.iterations = PASSWORD_ITERATIONS_DEFAULT;
        verifier.hashLength = CRYPTO_SHA256_DIGEST_SIZE;
        ok = Password_deriveSha256(password, verifier.salt, verifier.saltLength,
                                   verifier.iterations, verifier.hash, verifier.hashLength);
    } else {
        verifier.memoryKiB = ARGON2_MEMORY_DEFAULT;
        verifier.iterations = ARGON2_PASSES_DEFAULT;
        verifier.lanes = ARGON2_LANES_DEFAULT;
        verifier.hashLength = ARGON2_TAG_DEFAULT;
        Argon2Params params = {
            .memoryKiB = verifier.memoryKiB,
            .passes = verifier.iterations,
            .lanes = verifier.lanes,
            .tagBytes = verifier.hashLength
        };
        ok = Argon2id_derive(&params, (const u8*) password, passwordLen,
                             verifier.salt, verifier.saltLength, nullptr, 0u,
                             nullptr, 0u, verifier.hash, sizeof verifier.hash);
    }
    if (!ok) {
        memset(&verifier, 0, sizeof verifier);
        verifier.code = PASSWORD_CODE_DERIVATION_FAILED;
        return verifier;
    }
    verifier.code = PASSWORD_CODE_OK;
    return verifier;
}

bool PasswordVerifier_isValid(const PasswordVerifier *self) {
    if (self == nullptr)
        return false;
    if ((*self).code != PASSWORD_CODE_OK || (*self).saltLength < PASSWORD_SALT_MIN ||
        (*self).saltLength > PASSWORD_SALT_MAX || (*self).hashLength != 32u ||
        (*self).kdf == 0u)
        return false;
    if ((*self).kdf == PASSWORD_KDF_PBKDF2_SHA256)
        return (*self).iterations >= PASSWORD_ITERATIONS_MIN &&
               (*self).memoryKiB == 0u && (*self).lanes == 0u;
    if ((*self).kdf == PASSWORD_KDF_ARGON2ID)
        return (*self).memoryKiB != 0u && (*self).iterations != 0u &&
               (*self).lanes != 0u;
    return false;
}

bool PasswordVerifier_verify(const PasswordVerifier *self, const char *password) {
    if (!PasswordVerifier_isValid(self) || password == nullptr || password[0] == '\0') {
        THROW("PasswordVerifier_verify: invalid verifier or password");
        return false;
    }
    u8 derived[32] = {0};
    bool ok = false;
    bytes passwordLen = __builtin_strlen(password);
    if ((*self).kdf == PASSWORD_KDF_PBKDF2_SHA256) {
        ok = Password_deriveSha256(password, (*self).salt, (*self).saltLength,
                                   (*self).iterations, derived, sizeof derived);
    } else if ((*self).kdf == PASSWORD_KDF_ARGON2ID) {
        Argon2Params params = {
            .memoryKiB = (*self).memoryKiB,
            .passes = (*self).iterations,
            .lanes = (*self).lanes,
            .tagBytes = (*self).hashLength
        };
        ok = Argon2id_derive(&params, (const u8*) password, passwordLen,
                             (*self).salt, (*self).saltLength, nullptr, 0u,
                             nullptr, 0u, derived, sizeof derived);
    }
    bool same = ok && Crypto_constantTimeEquals(derived, (*self).hash, sizeof derived);
    memset(derived, 0, sizeof derived);
    return same;
}
