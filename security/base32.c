#include "security/base32.h"
#include "util/throw.h"
#include "util/annotation.h"

;;DEFINITION
/** RFC 4648 base32, the encoding every authenticator app reads a TOTP secret in.
 * A pure format algorithm: borrowed bytes in, caller-owned bytes out, no file, no
 * device, no state, no allocation. It exists in `security/` because its one use is
 * the setup secret; it would move to a format module unchanged if a second user
 * appeared. */
;;OVERVIEW
/** ========================================================================
 * MODULE: Base32 (security/base32.c)
 * FUNCTION REGISTRY (exports)
 *   Base32_encodedLength(byteLen) / Base32_decodedLength(charLen)
 *   Base32_encode(bytes, len, dest, cap, outTruncated)   dest-last, truncation-flagged
 *   Base32_decode(text, len, dest, cap, outLen)
 * PRIVATE HELPERS: decodeChar(c) — one alphabet character to 5 bits (-1 invalid)
 * CONTRACT: null input rejects through THROW; a too-small destination reports
 * truncation (encode) or rejects (decode); no allocation, no logging on success.
 * ======================================================================== */

static const char ALPHABET[] = BASE32_ALPHABET;

bytes Base32_encodedLength(bytes byteLen) { return ((byteLen + 4u) / 5u) * 8u; }
bytes Base32_decodedLength(bytes charLen) { return (charLen * 5u) / 8u; }

static int decodeChar(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a';
    if (c >= '2' && c <= '7')
        return 26 + (c - '2');
    return -1;
}

bool Base32_encode(const u8 *bytes, bytes len, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated != nullptr)
        *outTruncated = false;
    if ((bytes == nullptr && len != 0u) || dest == nullptr) {
        THROW("Base32_encode: null input");
        return false;
    }
    bytes need = Base32_encodedLength(len);
    if (cap < need + 1u) {
        if (outTruncated != nullptr)
            *outTruncated = true;
        THROW("Base32_encode: destination too small");
        return false;
    }
    bytes out = 0;
    for (bytes i = 0; i < len; i += 5u) {
        bytes take = len - i < 5u ? len - i : 5u;
        u64 group = 0u;
        for (bytes j = 0; j < 5u; ++j)
            group = (group << 8) | (u64) (j < take ? bytes[i + j] : 0u);
        for (unsigned j = 0; j < 8u; ++j)
            dest[out++] = ALPHABET[(group >> (35u - j * 5u)) & 0x1Fu];
        bytes pad = (5u - take) * 8u / 5u;
        for (bytes p = 0; p < pad; ++p)
            dest[out - 1u - p] = '=';
    }
    dest[out] = '\0';
    return true;
}

bool Base32_decode(const char *text, bytes len, u8 *dest, bytes cap, bytes *outLen) {
    if (outLen != nullptr)
        *outLen = 0u;
    if (text == nullptr || dest == nullptr) {
        THROW("Base32_decode: null input");
        return false;
    }
    bytes out = 0u;
    u32 acc = 0u;
    unsigned bits = 0u;
    for (bytes i = 0; i < len; ++i) {
        char c = text[i];
        if (c == '=')
            break;
        if (c == ' ' || c == '-' || c == '\n' || c == '\r' || c == '\t')
            continue;
        int value = decodeChar(c);
        if (value < 0) {
            THROW("Base32_decode: invalid character");
            return false;
        }
        acc = (acc << 5) | (u32) value;
        bits += 5u;
        if (bits >= 8u) {
            bits -= 8u;
            if (out >= cap) {
                THROW("Base32_decode: destination too small");
                return false;
            }
            dest[out++] = (u8) ((acc >> bits) & 0xFFu);
        }
    }
    if (outLen != nullptr)
        *outLen = out;
    return true;
}
