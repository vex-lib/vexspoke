#ifndef SECURITY_BASE32_H
#define SECURITY_BASE32_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// security/base32.h — RFC 4648 base32: the transport form of a TOTP setup secret.
//
// Pure bytes → bytes: no file, no device, no state, no allocation (the Pure Format
// Algorithm Law). A TOTP secret is 160 random bits shown to the user as base32
// because that is what `otpauth://` URIs and every authenticator app expect.
//
// Decoding tolerates grouping characters (space, '-') and case, and stops at '='.
// Any other invalid character is a cold rejection, never a silent skip.

#define BASE32_ALPHABET "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"

// Encoded length of `byteLen` bytes including '=' padding (0 for 0 bytes).
bytes Base32_encodedLength(bytes byteLen);
// Upper bound on the bytes a `charLen`-character payload can decode to, for sizing.
bytes Base32_decodedLength(bytes charLen);
// Write the base32 form into `dest` (NUL-terminated). False + *outTruncated when
// `cap` cannot hold it; on refusal `dest` is left untouched.
bool   Base32_encode(const u8 *bytes, bytes len, char *dest, bytes cap, bool *outTruncated);
// Decode `text` into `dest`; *outLen receives the exact byte count. False on an
// invalid character or an undersized destination.
bool   Base32_decode(const char *text, bytes len, u8 *dest, bytes cap, bytes *outLen);

#endif
