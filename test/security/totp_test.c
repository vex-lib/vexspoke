// Owner: security/totp.c — RFC 6238 TOTP: codes, clock window, replay refusal.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "security/totp.h"

// RFC 6238 Appendix B uses the ASCII secret "12345678901234567890" in SHA-1 mode.
static const uint8_t SECRET[] = "12345678901234567890";

int main(void) {
    struct { uint64_t t; uint32_t code; } vectors[] = {
        { 59u,          94287082u },
        { 1111111109u,  7081804u },
        { 1234567890u,  89005924u },
        { 20000000000u, 65353130u },
    };
    for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; ++i) {
        uint32_t code = Totp_code(SECRET, sizeof SECRET - 1, vectors[i].t, 8u,
                                  TOTP_STEP_SECONDS_DEFAULT);
        assert(code == vectors[i].code);
    }

    // Six digits is the last six of the eight-digit code.
    assert(Totp_code(SECRET, sizeof SECRET - 1, 59u, 6u, TOTP_STEP_SECONDS_DEFAULT) == 287082u);

    // Verification inside the window, with replay refusal recorded per user.
    uint64_t lastStep = 0;
    assert(Totp_verify(SECRET, sizeof SECRET - 1, 94287082u, 59u, 8u,
                       TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, &lastStep));
    assert(lastStep == 1u);                                   // 59 / 30
    // The same code, replayed, is refused for the rest of its validity.
    assert(!Totp_verify(SECRET, sizeof SECRET - 1, 94287082u, 59u, 8u,
                        TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, &lastStep));
    // A later step is accepted again.
    assert(Totp_verify(SECRET, sizeof SECRET - 1, 7081804u, 1111111109u, 8u,
                       TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, &lastStep));
    assert(lastStep == 1111111109u / TOTP_STEP_SECONDS_DEFAULT);

    // Clock skew: a code from the previous step still verifies with a window of 1.
    uint64_t fresh = 0;
    assert(Totp_verify(SECRET, sizeof SECRET - 1, 94287082u, 89u, 8u,
                       TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, &fresh));
    // ...and the next step's code does not, outside the window.
    uint32_t next = Totp_code(SECRET, sizeof SECRET - 1, 119u, 8u, TOTP_STEP_SECONDS_DEFAULT);
    assert(!Totp_verify(SECRET, sizeof SECRET - 1, next, 59u, 8u,
                        TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, nullptr));

    // A wrong code is refused.
    assert(!Totp_verify(SECRET, sizeof SECRET - 1, 12345678u, 59u, 8u,
                        TOTP_STEP_SECONDS_DEFAULT, TOTP_WINDOW_DEFAULT, nullptr));

    // Invalid digit counts and null secrets reject rather than compute.
    assert(Totp_code(SECRET, sizeof SECRET - 1, 59u, 5u, TOTP_STEP_SECONDS_DEFAULT) == 0u);
    assert(Totp_code(SECRET, sizeof SECRET - 1, 59u, 9u, TOTP_STEP_SECONDS_DEFAULT) == 0u);
    assert(Totp_code(nullptr, 0, 59u, 6u, TOTP_STEP_SECONDS_DEFAULT) == 0u);
    assert(!Totp_verify(nullptr, 0, 1u, 59u, 6u, TOTP_STEP_SECONDS_DEFAULT, 1, nullptr));
    assert(!Totp_verify(SECRET, sizeof SECRET - 1, 1u, 59u, 6u, 0u, 1, nullptr));
    printf("totp_test: PASS\n");
    return 0;
}
