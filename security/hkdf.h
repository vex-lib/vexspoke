#ifndef SECURITY_HKDF_H
#define SECURITY_HKDF_H

#include "vex.h"

// security/hkdf.h — HKDF (RFC 5869) over HMAC-SHA256.
//
// HKDF is the "key-cutting machine": extract makes one pseudorandom key from
// mixed input, and expand turns that key into as many labeled subkeys as needed.
// It adds no entropy; it adds SEPARATION — the same master key used for
// encryption, indexing and signing becomes three unrelated keys because each
// expansion carries a different `info` label. That is the domain separation the
// sealed/indexed field modifiers need.
//
// Correctness is proved by the RFC 5869 appendix A vectors in
// tests/vexspoke/security/hkdf_test.c.

#define HKDF_HASH_SIZE   32u                        // HMAC-SHA256 output
#define HKDF_MAX_OUTPUT  (255u * HKDF_HASH_SIZE)    // the RFC's ceiling

// PRK = HMAC-SHA256(salt, IKM). A null or empty salt uses 32 zero bytes.
fn Hkdf_extract(const u8 *salt, bytes saltLen,
                  const u8 *ikm, bytes ikmLen,
                  u8 prk[HKDF_HASH_SIZE]);
// OKM = T(1) || T(2) || ... . info may be null when infoLen is 0.
fn Hkdf_expand(const u8 prk[HKDF_HASH_SIZE], const u8 *info, bytes infoLen,
                 u8 *okm, bytes okmLen);
// extract then expand.
fn Hkdf(const u8 *salt, bytes saltLen, const u8 *ikm, bytes ikmLen,
          const u8 *info, bytes infoLen, u8 *okm, bytes okmLen);

#endif
