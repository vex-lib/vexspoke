// Owner: security/password.c — versioned PBKDF2/Argon2id verifier records.
#include "security/password.h"
#include "security/argon2.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    PasswordVerifier pbkdf2 = Password_createVerifier(PASSWORD_KDF_PBKDF2_SHA256, "correct horse");
    assert(PasswordVerifier_isValid(&pbkdf2));
    assert(PasswordVerifier_verify(&pbkdf2, "correct horse"));
    assert(!PasswordVerifier_verify(&pbkdf2, "wrong horse"));
    assert(pbkdf2.kdf == PASSWORD_KDF_PBKDF2_SHA256);
    assert(pbkdf2.memoryKiB == 0u);
    assert(pbkdf2.lanes == 0u);

    PasswordVerifier argon2 = Password_createVerifier(PASSWORD_KDF_ARGON2ID, "correct horse");
    assert(PasswordVerifier_isValid(&argon2));
    assert(PasswordVerifier_verify(&argon2, "correct horse"));
    assert(!PasswordVerifier_verify(&argon2, "wrong horse"));
    assert(argon2.kdf == PASSWORD_KDF_ARGON2ID);
    assert(argon2.memoryKiB == ARGON2_MEMORY_DEFAULT);
    assert(argon2.iterations == ARGON2_PASSES_DEFAULT);
    assert(argon2.lanes == ARGON2_LANES_DEFAULT);

    PasswordVerifier tampered = argon2;
    tampered.kdf = 99u;
    assert(!PasswordVerifier_isValid(&tampered));
    tampered = argon2;
    tampered.memoryKiB = 0u;
    assert(!PasswordVerifier_isValid(&tampered));
    tampered = pbkdf2;
    tampered.iterations = PASSWORD_ITERATIONS_MIN - 1u;
    assert(!PasswordVerifier_isValid(&tampered));

    PasswordVerifier invalid = Password_createVerifier((PasswordKdf) 99u, "password");
    assert(invalid.code == PASSWORD_CODE_UNSUPPORTED_KDF);
    assert(!PasswordVerifier_isValid(&invalid));

    printf("password_verifier_test: PASS\n");
    return 0;
}
