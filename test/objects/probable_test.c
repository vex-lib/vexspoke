// vexspoke/test/objects/probable_test.c — the Probable class _test.

#include <stdio.h>

#include "objects/probable.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks Probable fields, weighted draw behavior, and null-safe accessors.
int main() {
    Probable *p = Probable_3((pointer) 0xABCD, 3, 10);
    CHECK(p != nullptr);
    CHECK(Probable_object(p) == (pointer) 0xABCD);
    CHECK(Probable_weight(p) == 3);
    CHECK(Probable_total(p) == 10);

    Probable_setObject(p, (pointer) 0x1234);
    CHECK(Probable_object(p) == (pointer) 0x1234);
    Probable_setWeight(p, 7);
    CHECK(Probable_weight(p) == 7);
    Probable_setTotal(p, 10);
    CHECK(Probable_total(p) == 10);

    // Probable_get rolls the weighted choice: object on a hit, 0 on a miss.
    pointer v = Probable_get(p);
    CHECK(v == (pointer) 0x1234 || v == 0);

    Probable_free(p);

    // Null-safety.
    CHECK(Probable_object(nullptr) == 0);
    CHECK(Probable_weight(nullptr) == 0);
    CHECK(Probable_total(nullptr) == 0);
    Probable_setObject(nullptr, 1);
    Probable_setWeight(nullptr, 1);
    Probable_setTotal(nullptr, 1);
    CHECK(Probable_get(nullptr) == 0);
    Probable_free(nullptr);

    if (g_failures == 0) {
        printf("probable_test: all assertions held\n");
        return 0;
    }
    printf("probable_test: %d FAILURES\n", g_failures);
    return 1;
}
