#ifndef SECURITY_CHACHA20POLY1305_H
#define SECURITY_CHACHA20POLY1305_H

#include "vex.h"

// security/chacha20poly1305.h — ChaCha20-Poly1305 AEAD (RFC 8439).
//
// The construction that makes encryption safe to use: ChaCha20 encrypts, and
// Poly1305 authenticates the ciphertext (and any associated data) so a single
// flipped bit is detected. The Poly1305 key is derived fresh from ChaCha20 for
// every message, which is what keeps Poly1305's one-time-key rule honest.
//
// This is the mechanism behind the `client-sealed` and `server-sealed` field
// modifiers: the caller never touches ChaCha20 or Poly1305 directly, so the
// nonce and one-time key cannot be mismanaged at the call site.
//
// Correctness is proved by the RFC 8439 section 2.8.2 vector in
// tests/vexspoke/security/chacha20poly1305_test.c.

#define CHACHA20POLY1305_KEY_SIZE   32u
#define CHACHA20POLY1305_NONCE_SIZE 12u
#define CHACHA20POLY1305_TAG_SIZE   16u

// Encrypt plaintextLen bytes into ciphertext and write the 16-byte tag. aad may
// be null only when aadLen is 0; plaintext may be null only when plaintextLen is 0.
bool ChaCha20Poly1305_encrypt(const u8 key[CHACHA20POLY1305_KEY_SIZE],
                              const u8 nonce[CHACHA20POLY1305_NONCE_SIZE],
                              const u8 *aad, bytes aadLen,
                              const u8 *plaintext, bytes plaintextLen,
                              u8 *ciphertext, u8 tag[CHACHA20POLY1305_TAG_SIZE]);

// Verify the tag, then decrypt ciphertextLen bytes into plaintext. On a tag
// mismatch the plaintext buffer is zeroed and false is returned; a forged
// message never yields plaintext.
bool ChaCha20Poly1305_decrypt(const u8 key[CHACHA20POLY1305_KEY_SIZE],
                              const u8 nonce[CHACHA20POLY1305_NONCE_SIZE],
                              const u8 *aad, bytes aadLen,
                              const u8 *ciphertext, bytes ciphertextLen,
                              const u8 tag[CHACHA20POLY1305_TAG_SIZE],
                              u8 *plaintext);

#endif
