#ifndef SECURITY_SECURE_RANDOM_H
#define SECURITY_SECURE_RANDOM_H

#include "vex.h"

// security/secure_random.h — OS-backed cryptographically secure randomness.
//
// SecureRandom sources entropy from the operating system's CSPRNG and NEVER
// from a caller-supplied seed (there is deliberately no SecureRandom_1). It is
// the non-reproducible counterpart to util/random.h's Random, which is seedable
// and deterministic ON PURPOSE. Use it for tokens, keys, nonces, salts, session
// ids, and anything else an attacker must not predict.
//
// Backends by host:
//   Apple / BSD : arc4random_buf (the OS CSPRNG)
//   Linux       : getrandom(2), falling back to /dev/urandom
//   Windows     : BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG)
//   other UNIX  : /dev/urandom
//
// THREAD CONTRACT: each handle is independent; a single handle is not
// synchronized. One handle per thread, or one per call site.

typedef struct SecureRandom SecureRandom;

// New generator handle (no seed argument — entropy comes from the OS).
SecureRandom *SecureRandom_0(void);
#define SecureRandom(...) CONSTRUCTOR_DISPATCH(SecureRandom, __VA_ARGS__)
void SecureRandom_free(SecureRandom *self);

// Fill dest[0..len) with cryptographically secure Bytes. Returns false when the
// OS source is unavailable (dest untouched). len 0 is a successful no-op.
bool SecureRandom_bytes(SecureRandom *self, void *dest, bytes len);

// Convenience scalar draws; 0 on OS failure (prefer SecureRandom_bytes when the
// caller must distinguish failure from a legitimate zero).
u64 SecureRandom_u64(SecureRandom *self);
u32 SecureRandom_u32(SecureRandom *self);

// Telemetry: number of draws issued through this handle.
u64 SecureRandom_draws(const SecureRandom *self);

#endif
