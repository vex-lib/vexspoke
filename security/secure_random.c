#include "vex.h"

#include <stdlib.h>

#include "security/secure_random.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SecureRandom
 * ============================================================================
 * The OS-backed cryptographically secure random source. It holds no algorithmic
 * state of its own — every draw is delegated to the host's CSPRNG (Apple/BSD
 * arc4random_buf, Linux getrandom, Windows BCryptGenRandom, or /dev/urandom).
 * Delegating rather than rolling a DRBG is deliberate: the OS already implements
 * a seeded, reseeded, forward-secret CSPRNG, so there is nothing to get wrong.
 * A handle exists only to carry a draw counter (telemetry) and to give the
 * caller an explicit, non-seedable object. The struct is a cold heap handle.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SecureRandom (security/secure_random.c)
 * ============================================================================
 * OS-backed cryptographically secure randomness. Non-reproducible by design;
 * it is the counterpart to util/random.h's seedable Random.
 *
 * STRUCT FIELDS (Mirroring security/secure_random.h):
 * ----------------------------------------------------------------------------
 *   SecureRandom {
 *     u64 draws; // count of draws issued (telemetry)
 *   }
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   osFill(dest, len)       : fill from the host CSPRNG; false when unavailable
 *   fillFromUrandom(dest, n): /dev/urandom fallback (POSIX)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - SecureRandom_0(void)
 *
 * Core Functions:
 *   - SecureRandom_free(self)
 *   - SecureRandom_bytes(self, dest, len)
 *   - SecureRandom_u64(self)
 *   - SecureRandom_u32(self)
 *
 * Getters:
 *   - SecureRandom_draws(self)
 * ============================================================================
 */

// --- OS CSPRNG backends -----------------------------------------------------
#if defined(VEX_PLATFORM_APPLE)
    #define SR_BACKEND_ARC4RANDOM 1
#elif defined(VEX_PLATFORM_LINUX)
    #define SR_BACKEND_GETRANDOM 1
    #include <errno.h>
    #include <fcntl.h>
    #include <sys/random.h>
    #include <unistd.h>
#elif defined(VEX_PLATFORM_WINDOWS)
    #define SR_BACKEND_BCRYPT 1
    #include <windows.h>
    #include <bcrypt.h>
#else
    #define SR_BACKEND_URANDOM 1
    #include <fcntl.h>
    #include <unistd.h>
#endif

#if defined(SR_BACKEND_GETRANDOM) || defined(SR_BACKEND_URANDOM)
// POSIX fallback: bounded read loop over /dev/urandom.
static bool fillFromUrandom(void *dest, bytes len) {
    i32 fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return false;
    u8 *p = (u8*) dest;
    bytes got = 0;
    while (got < len) {
        ssize_t n = read(fd, p + got, len - got);
        if (n <= 0) {
            close(fd);
            return false;
        }
        got += (bytes) n;
    }
    close(fd);
    return true;
}
#endif

static bool osFill(void *dest, bytes len) {
    if (len == 0)
        return true;
    if (dest == nullptr)
        return false;

#if defined(SR_BACKEND_ARC4RANDOM)
    arc4random_buf(dest, len);
    return true;
#elif defined(SR_BACKEND_GETRANDOM)
    u8 *p = (u8*) dest;
    bytes got = 0;
    while (got < len) {
        ssize_t n = getrandom(p + got, len - got, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return fillFromUrandom(p + got, len - got);
        }
        got += (bytes) n;
    }
    return true;
#elif defined(SR_BACKEND_BCRYPT)
    NTSTATUS status = BCryptGenRandom(nullptr, (PUCHAR) dest, (ULONG) len,
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status == 0;
#else
    return fillFromUrandom(dest, len);
#endif
}

// --- Public API -------------------------------------------------------------
struct SecureRandom {
    u64 draws; // count of draws issued (telemetry)
};

SecureRandom *SecureRandom_0(void) {
    SecureRandom *self = (SecureRandom*) calloc(1, sizeof(SecureRandom));
    return self;
}

void SecureRandom_free(SecureRandom *self) {
    free(self);
}

bool SecureRandom_bytes(SecureRandom *self, void *dest, bytes len) {
    if (self == nullptr)
        return false;
    if (!osFill(dest, len))
        return false;
    (*self).draws++;
    return true;
}

u64 SecureRandom_u64(SecureRandom *self) {
    u64 value = 0;
    if (!SecureRandom_bytes(self, &value, sizeof value))
        return 0;
    return value;
}

u32 SecureRandom_u32(SecureRandom *self) {
    u32 value = 0;
    if (!SecureRandom_bytes(self, &value, sizeof value))
        return 0;
    return value;
}

u64 SecureRandom_draws(const SecureRandom *self) {
    return self ? (*self).draws : 0;
}
