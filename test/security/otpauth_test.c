// Owner: security/otpauth.c — the otpauth:// setup URI every authenticator reads.
#include "vex.h"

#include <assert.h>

#include "security/otpauth.h"

int main(void) {
    char uri[512];
    bool cut = true;
    // A 5-byte secret is "fooba", whose base32 form is the RFC 4648 vector MZXW6YTB,
    // so the whole URI is knowable exactly: '@' and any delimiter are escaped.
    const u8 secret[] = { 'f', 'o', 'o', 'b', 'a' };
    assert(OtpAuth_buildUri("Darkbase", "vex@example.com", secret, sizeof secret,
                            6u, 30u, uri, sizeof uri, &cut));
    assert(!cut);
    printf("actual:   %s\n", uri);
    assert(strcmp(uri,
        "otpauth://totp/Darkbase:vex%40example.com?secret=MZXW6YTB"
        "&issuer=Darkbase&algorithm=SHA1&digits=6&period=30") == 0);

    // A space and a colon in a name are escaped, never left to rewrite the query.
    assert(OtpAuth_buildUri("My DB", "a:b.sh", secret, sizeof secret, 8u, 60u, uri, sizeof uri, &cut));
    assert(strstr(uri, "My%20DB:a%3Ab") != nullptr);
    assert(strstr(uri, "digits=8&period=60") != nullptr);

    // Seven digits and a non-default step are carried through.
    assert(OtpAuth_buildUri("I", "a", secret, sizeof secret, 7u, 15u, uri, sizeof uri, &cut));
    assert(strstr(uri, "digits=7&period=15") != nullptr);

    // A buffer that cannot hold it reports truncation and never half-writes.
    char tiny[16];
    memset(tiny, 'x', sizeof tiny);
    assert(!OtpAuth_buildUri("Darkbase", "vex", secret, sizeof secret, 6u, 30u,
                             tiny, sizeof tiny, &cut));
    assert(cut);

    // Invalid input rejects rather than emitting a URI an app would misread.
    assert(!OtpAuth_buildUri(nullptr, "vex", secret, sizeof secret, 6u, 30u, uri, sizeof uri, &cut));
    assert(!OtpAuth_buildUri("Darkbase", nullptr, secret, sizeof secret, 6u, 30u, uri, sizeof uri, &cut));
    assert(!OtpAuth_buildUri("Darkbase", "vex", nullptr, 0u, 6u, 30u, uri, sizeof uri, &cut));
    assert(!OtpAuth_buildUri("Darkbase", "vex", secret, sizeof secret, 5u, 30u, uri, sizeof uri, &cut));
    assert(!OtpAuth_buildUri("Darkbase", "vex", secret, sizeof secret, 9u, 30u, uri, sizeof uri, &cut));
    assert(!OtpAuth_buildUri("Darkbase", "vex", secret, sizeof secret, 6u, 0u, uri, sizeof uri, &cut));
    printf("otpauth_test: PASS\n");
    return 0;
}
