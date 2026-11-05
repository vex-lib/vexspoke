// vexspoke/test/c23/equals_test.c — the Equals class _test.
//
// Proves c23/equals.c isEqual: identity, then the block identity header (type +
// length), then payload Bytes; foreign pointers prove identity only.
//
// STATED GAP: isEquallyNamed's success path needs a live SymbolTable and is
// exercised by the relational owner test; here only its null-guard is pinned.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "c23/equals.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks identity, typed block contents, and the registry comparison guard.
int main(void) {
    int a = 1;
    int b = 2;

    // Identity holds even for foreign pointers; distinct foreigns never match.
    CHECK(isEqual(nullptr, nullptr));
    CHECK(isEqual(&a, &a));
    CHECK(!isEqual(&a, &b));
    CHECK(!isEqual(nullptr, &a));
    CHECK(!isEqual(&a, nullptr));

    // Allocated blocks: equal type + length + Bytes match.
    void *x = Memory_alloc(7001u, 8);
    void *y = Memory_alloc(7001u, 8);
    CHECK(x && y);
    memset(x, 0, 8);
    memset(y, 0, 8);
    CHECK(isEqual(x, y));

    // A payload difference breaks content equality.
    memset(y, 1, 1);
    CHECK(!isEqual(x, y));
    memset(y, 0, 1);
    CHECK(isEqual(x, y));

    // Different type id -> not equal, even with equal Bytes.
    void *z = Memory_alloc(7002u, 8);
    CHECK(z);
    memset(z, 0, 8);
    CHECK(!isEqual(x, z));

    // Different length -> not equal.
    void *big = Memory_alloc(7001u, 16);
    CHECK(big);
    memset(big, 0, 16);
    CHECK(!isEqual(x, big));

    Memory_free(x);
    Memory_free(y);
    Memory_free(z);
    Memory_free(big);

    // Registry form: null guards.
    CHECK(!isEquallyNamed(nullptr, "a", "b.sh"));
    CHECK(!isEquallyNamed(nullptr, nullptr, nullptr));

    if (g_failures == 0) {
        printf("equals_test: all assertions held\n");
        return 0;
    }
    printf("equals_test: %d FAILURES\n", g_failures);
    return 1;
}
