// vexspoke/test/objects/global_test.c — the Global class _test.
//
// Atomic global pointer/value: get/set round trip + compare-and-set.

#include <stdio.h>

#include "objects/global.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks Global get/set, compare-and-set, and null-safe operations.
int main(void) {
    Global *g = Global_1(10);
    CHECK(g != nullptr);
    CHECK(Global_get(g) == 10);

    Global_set(g, 20);
    CHECK(Global_get(g) == 20);

    CHECK(Global_compareAndSet(g, 20, 30));
    CHECK(Global_get(g) == 30);
    CHECK(!Global_compareAndSet(g, 20, 40)); // stale expected
    CHECK(Global_get(g) == 30);

    Global_free(g);

    // Null-safety.
    CHECK(Global_get(nullptr) == 0);
    Global_set(nullptr, 1);
    CHECK(!Global_compareAndSet(nullptr, 0, 1));
    Global_free(nullptr);

    if (g_failures == 0) {
        printf("global_test: all assertions held\n");
        return 0;
    }
    printf("global_test: %d FAILURES\n", g_failures);
    return 1;
}
