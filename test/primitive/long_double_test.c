// vexspoke/test/primitive/long_f64_test.c — the Longf64 class _test.
//
// Composite: int64 (offset 0) + d64 (offset 8); the API reads/writes int64.

#include <stdint.h>
#include <stdio.h>

#include "primitive/long_f64.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Longf64's composite layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(Longf64_init());

    void *p = Longf64_alloc();
    CHECK(p != nullptr);
    CHECK(Longf64_get(p) == 0);
    CHECK(Longf64_type(p) == ID_LONG_f64);
    CHECK(Longf64_length(p) == 16);

    Longf64_set(p, 0x0123456789ABCDEFLL);
    CHECK(Longf64_get(p) == 0x0123456789ABCDEFLL);
    Longf64_set(p, -1);
    CHECK(Longf64_get(p) == -1);

    void *c = Longf64_allocWithValues(7, 2.5);
    CHECK(c != nullptr);
    CHECK(Longf64_get(c) == 7); // v1 (int64) landed at offset 0

    void *r = Longf64_2(4, 0.5);
    CHECK(r != nullptr && Longf64_get(r) == 4);
    void *s = Longf64_0();
    CHECK(s != nullptr && Longf64_type(s) == ID_LONG_f64);

    Longf64_set(p, 11);
    CHECK(Longf64_compareAndSet(p, 11, 22));
    CHECK(Longf64_get(p) == 22);
    CHECK(!Longf64_compareAndSet(p, 11, 33));
    CHECK(Longf64_get(p) == 22);

    CHECK(Longf64_allocArray(0) == nullptr);
    void *arr = Longf64_array(4);
    CHECK(arr != nullptr);
    CHECK((Longf64_type(arr) & MASK_CLASS) == ID_LONG_f64);
    CHECK(Longf64_length(arr) >= 4);

    CHECK(Longf64_get(nullptr) == 0);
    Longf64_set(nullptr, 5);
    CHECK(!Longf64_compareAndSet(nullptr, 0, 1));
    CHECK(Longf64_type(nullptr) == 0);
    CHECK(Longf64_length(nullptr) == 0);
    Longf64_free(nullptr);

    Longf64_free(p);
    Longf64_free(c);
    Longf64_free(r);
    Longf64_free(s);
    Longf64_free(arr);
    Longf64_shutdown();

    if (g_failures == 0) {
        printf("long_f64_test: all assertions held\n");
        return 0;
    }
    printf("long_f64_test: %d FAILURES\n", g_failures);
    return 1;
}
