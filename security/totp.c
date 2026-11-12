#include "vex.h"

#include "security/totp.h"
#include "security/hmac.h"
#include "security/crypto.h"

;;DEFINITION
/** TOTP: a shared per-user secret plus a counter derived from the clock. The code
 * is a truncated HMAC-SHA1 over that counter, which is why the digest module keeps
 * SHA-1 around. This file owns the two behaviours an authenticator app cannot
 * provide for you: a bounded acceptance window for clock skew, and replay refusal,
 * because a code that was valid once must not be valid twice. */
;;OVERVIEW
/** ========================================================================
 * MODULE: Totp (security/totp.c)
 * FUNCTION REGISTRY (exports)
 *   Totp_code(secret, secretLen, unixTime, digits, stepSeconds)
 *   Totp_verify(secret, secretLen, code, unixTime, digits, stepSeconds, window, lastStep)
 * PRIVATE HELPERS:
 *   digitsModulus(digits)      the 10^digits modulus for 6..8, else 0
 *   stepCounter(step, into)    big-endian 8-byte counter
 * CONTRACT: a null secret or an out-of-range digit count rejects through THROW;
 * comparison uses Crypto_constantTimeEquals; no allocation, no logging on success.
 * ======================================================================== */

static u32 digitsModulus(u32 digits) {
    switch (digits) {
        case 6u: return 1000000u;
        case 7u: return 10000000u;
        case 8u: return 100000000u;
        default: return 0u;
    }
}

static void stepCounter(u64 step, u8 out[8]) {
    for (u32 i = 0; i < 8u; ++i)
        out[i] = (u8) (step >> (56u - i * 8u));
}

u32 Totp_code(const u8 *secret, bytes secretLen, u64 unixTime,
                   u32 digits, u32 stepSeconds) {
    u32 modulus = digitsModulus(digits);
    if (secret == nullptr || secretLen == 0u || modulus == 0u || stepSeconds == 0u) {
        THROW("Totp_code: invalid secret, digits or step");
        return 0u;
    }
    u8 counter[8];
    stepCounter(unixTime / stepSeconds, counter);
    u8 mac[HMAC_SHA1_DIGEST_SIZE];
    Hmac_sha1(secret, secretLen, counter, sizeof counter, mac);
    u32 offset = mac[HMAC_SHA1_DIGEST_SIZE - 1u] & 0x0Fu;
    u32 binary = ((u32) (mac[offset] & 0x7Fu) << 24) |
                      ((u32) mac[offset + 1u] << 16) |
                      ((u32) mac[offset + 2u] << 8) |
                      (u32) mac[offset + 3u];
    u32 code = binary % modulus;
    for (u32 i = 0; i < sizeof mac; ++i)
        mac[i] = 0u;
    return code;
}

bool Totp_verify(const u8 *secret, bytes secretLen, u32 code, u64 unixTime,
                 u32 digits, u32 stepSeconds, i32 window, u64 *ioLastAcceptedStep) {
    if (secret == nullptr || secretLen == 0u || stepSeconds == 0u || digitsModulus(digits) == 0u) {
        THROW("Totp_verify: invalid secret, digits or step");
        return false;
    }
    i32 span = window < 0 ? 0 : window;
    u64 step = unixTime / stepSeconds;
    for (i32 offset = -span; offset <= span; ++offset) {
        if (offset < 0 && (u64) (-offset) > step)
            continue;
        u64 candidate = offset < 0 ? step - (u64) (-offset) : step + (u64) offset;
        u32 expected = Totp_code(secret, secretLen, candidate * stepSeconds, digits, stepSeconds);
        if (!Crypto_constantTimeEquals(&expected, &code, sizeof expected))
            continue;
        if (ioLastAcceptedStep != nullptr) {
            if (candidate <= *ioLastAcceptedStep)
                return false;   // replay: this code was already accepted
            *ioLastAcceptedStep = candidate;
        }
        return true;
    }
    return false;
}
