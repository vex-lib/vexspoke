// vexspoke/test/objects/local_test.c — the Local class _test.
//
// Thread-local slot table: unset reads 0, writes grow the table on demand.

#include <stdio.h>

#include "objects/local.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks Local thread-slot reads, writes, growth, and null safety.
int main() {
    Local *l = Local_0();
    CHECK(l != nullptr);
    CHECK(Local_get(l, 0) == 0); // unset -> 0

    Local_set(l, 0, 5);
    CHECK(Local_get(l, 0) == 5);

    Local_set(l, 100, 7); // grows on demand
    CHECK(Local_get(l, 100) == 7);
    CHECK(Local_get(l, 0) == 5); // untouched

    Local_free(l);

    // Null-safety.
    CHECK(Local_get(nullptr, 0) == 0);
    Local_set(nullptr, 0, 1);
    Local_free(nullptr);

    if (g_failures == 0) {
        printf("local_test: all assertions held\n");
        return 0;
    }
    printf("local_test: %d FAILURES\n", g_failures);
    return 1;
}
