#ifndef SECURITY_SECURITY_H
#define SECURITY_SECURITY_H

// security/security.h — THE security directory header.
// One include for the security surface: argon2, base32, blake2b, chacha20,
// chacha20poly1305, crypto, hkdf, hmac, keychain, otpauth, password,
// poly1305, secure_random, sha1, totp, touchid.
// Sub-headers stay includable individually; this header is the door engine
// code uses.

#include "vex.h"
#include "security/argon2.h"
#include "security/base32.h"
#include "security/blake2b.h"
#include "security/chacha20.h"
#include "security/chacha20poly1305.h"
#include "security/crypto.h"
#include "security/hkdf.h"
#include "security/hmac.h"
#include "security/keychain.h"
#include "security/otpauth.h"
#include "security/password.h"
#include "security/poly1305.h"
#include "security/secure_random.h"
#include "security/sha1.h"
#include "security/totp.h"
#include "security/touchid.h"

#endif // SECURITY_SECURITY_H
