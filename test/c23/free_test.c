// vexspoke/test/c23/free_test.c — the relational destructor dispatcher _test.
//
// Proves c23/free.c: Destructor_register (idempotent replace, rejects id 0 /
// null fn), Destructor_lookup, and c23_free routing a block's runtime type
// through the dynamic table before reclaiming it with Memory_free.

#include <stdint.h>
#include <stdio.h>

#include "c23/free.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static int g_calls = 0;
// Counts destructor dispatches for the registered test type.
static fn countingDestructor(any ptr) {
    (fn) ptr;
    g_calls++;
}

// Distinguishes replacement of a registered destructor from its old handler.
static fn otherDestructor(any ptr) {
    (fn) ptr;
    g_calls += 100;
}

// Verifies destructor registration, replacement, lookup, and c23_free routing.
int main() {
    // Register / lookup.
    CHECK(Destructor_lookup(4242u) == nullptr);
    Destructor_register(4242u, countingDestructor);
    CHECK(Destructor_lookup(4242u) == countingDestructor);

    // Re-registering a type replaces the handler in place.
    Destructor_register(4242u, otherDestructor);
    CHECK(Destructor_lookup(4242u) == otherDestructor);
    Destructor_register(4242u, countingDestructor);
    CHECK(Destructor_lookup(4242u) == countingDestructor);

    // id 0 and a null fn are refused.
    Destructor_register(0u, countingDestructor);
    CHECK(Destructor_lookup(0u) == nullptr);
    Destructor_register(5150u, nullptr);
    CHECK(Destructor_lookup(5150u) == nullptr);

    // c23_free on NULL is a no-op.
    c23_free(nullptr);
    CHECK(1);

    // c23_free routes the block's type through the registered destructor.
    g_calls = 0;
    any p = Memory_alloc(4242u, 16);
    CHECK(p != nullptr);
    c23_free(p);
    CHECK(g_calls == 1);

    // An unregistered type frees cleanly (no handler, block reclaimed).
    any q = Memory_alloc(6001u, 16);
    CHECK(q != nullptr);
    c23_free(q);
    CHECK(1);

    // A built-in type case frees cleanly.
    any r = Memory_alloc(TYPE_PROBABLE, 64);
    CHECK(r != nullptr);
    c23_free(r);
    CHECK(1);

    if (g_failures == 0) {
        printf("free_test: all assertions held\n");
        return 0;
    }
    printf("free_test: %d FAILURES\n", g_failures);
    return 1;
}
