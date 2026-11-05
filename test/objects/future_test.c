// vexspoke/test/objects/future_test.c — the Future class _test.
//
// Single-assignment: get before set is 0, set once succeeds, a second set is
// refused and the value is preserved.

#include <stdio.h>

#include "objects/future.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks Future's single-assignment transition and preserved value on rejection.
int main(void) {
    Future *f = Future_0();
    CHECK(f != nullptr);
    CHECK(!Future_isGiven(f));
    CHECK(Future_get(f) == 0);

    CHECK(Future_setDesiredValue(f, 42));
    CHECK(Future_isGiven(f));
    CHECK(Future_get(f) == 42);

    CHECK(!Future_setDesiredValue(f, 99)); // already fulfilled
    CHECK(Future_get(f) == 42);            // preserved

    Future_free(f);

    // Null-safety.
    CHECK(!Future_isGiven(nullptr));
    CHECK(Future_get(nullptr) == 0);
    CHECK(!Future_setDesiredValue(nullptr, 1));
    Future_free(nullptr);

    if (g_failures == 0) {
        printf("future_test: all assertions held\n");
        return 0;
    }
    printf("future_test: %d FAILURES\n", g_failures);
    return 1;
}
