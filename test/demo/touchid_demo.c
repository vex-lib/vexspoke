// touchid_demo/test.c — Secure TouchID demo using the C23 token API.
// Compiles on macOS: clang -I src -framework Foundation -framework LocalAuthentication -o touchid_demo touchid_demo/test.c src/security/touchid_cocoa.m
// Compiles stub:     clang -I src -o touchid_demo touchid_demo/test.c src/security/touchid.c

#include "vex.h"
#include "security/touchid.h" // not "security/touchid.h" bc its overriding the c secutiry

#include <stdio.h>

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Touchid_demo (projects/vexspoke/touchid_demo/test.c)
 * ============================================================================
 * Secure TouchID demo using the C23 token API.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main()
 *   - printf(===\n")
 *   - TouchID_discard(tok)
 * ============================================================================
 */


// Demonstrates authenticate-once token verification, discard, and prompt flow.
int main() {
    printf("=== Secure TouchID token _test ===\n");

    /* 1. Start auth session */
    TouchIDToken tok = TouchID_authenticate("Confirm operation X");
    if (!tok.magic[0] && !tok.magic[1]) {
        printf("Auth was cancelled or rejected (returned null token).\n");
        return 1;
    }
    printf("Auth initiated. Token ready.\n");

    /* 2. CRITICAL: verify exactly once. Never write `if (TouchID_authenticate(...))`. */
    bool ok = TouchID_verify(tok);
    if (!ok) {
        printf("Token verification failed (consumed or invalid).\n");
        TouchID_discard(tok);
        return 1;
    }
    printf("Token verified — user authenticated.\n");

    /* 3. Perform the operation */
    printf(">>> Operation permitted. Performing... <<<\n");

    /* 4. Cleanup */
    TouchID_discard(tok);
    printf("Token discarded.\n");

    /* 5. Fresh second auth (must restart session) */
    TouchIDToken tok2 = TouchID_authenticate("Confirm operation Y");
    if (!tok2.magic[0] && !tok2.magic[1]) {
        printf("Second auth cancelled or rejected.\n");
        return 1;
    }
    if (!TouchID_verify(tok2)) {
        printf("Second token verification failed.\n");
        TouchID_discard(tok2);
        return 1;
    }
    printf("Second auth also verified (fresh session).\n");
    TouchID_discard(tok2);

    /* 6. Convenience 1-line TouchID_prompt _test */
    printf("--- Testing TouchID_prompt convenience function ---\n");
    if (TouchID_prompt("Confirm operation Z (1-line prompt)")) {
        printf("TouchID_prompt succeeded — operation Z permitted.\n");
    } else {
        printf("TouchID_prompt cancelled or failed.\n");
        return 1;
    }

    printf("=== All checks passed ===\n");
    return 0;
}
