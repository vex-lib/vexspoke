// vexspoke/test/primitive/int_f64_test.c — the Intf64 class _test.
//
// Composite: int32 (offset 0) + d64 (offset 4); the API reads/writes int64.

#include <stdint.h>
#include <stdio.h>

#include "primitive/int_f64.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Intf64's packed two-field layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(Intf64_init());

    void *p = Intf64_alloc();
    CHECK(p != nullptr);
    CHECK(Intf64_get(p) == 0);
    CHECK(Intf64_type(p) == ID_INT_f64);
    CHECK(Intf64_length(p) == 16);

    Intf64_set(p, 0x0123456789ABCDEFLL);
    CHECK(Intf64_get(p) == 0x0123456789ABCDEFLL);
    Intf64_set(p, -1);
    CHECK(Intf64_get(p) == -1);

    void *c = Intf64_allocWithValues(7, 2.5);
    CHECK(c != nullptr);
    CHECK((Intf64_get(c) & 0xFFFFFFFFLL) == 7); // v1 landed at offset 0

    void *r = Intf64_2(4, 0.5);
    CHECK(r != nullptr);
    CHECK((Intf64_get(r) & 0xFFFFFFFFLL) == 4);
    void *s = Intf64_0();
    CHECK(s != nullptr && Intf64_type(s) == ID_INT_f64);

    Intf64_set(p, 11);
    CHECK(Intf64_compareAndSet(p, 11, 22));
    CHECK(Intf64_get(p) == 22);
    CHECK(!Intf64_compareAndSet(p, 11, 33));
    CHECK(Intf64_get(p) == 22);

    CHECK(Intf64_allocArray(0) == nullptr);
    void *arr = Intf64_array(4);
    CHECK(arr != nullptr);
    CHECK((Intf64_type(arr) & MASK_CLASS) == ID_INT_f64);
    CHECK(Intf64_length(arr) >= 4);

    CHECK(Intf64_get(nullptr) == 0);
    Intf64_set(nullptr, 5);
    CHECK(!Intf64_compareAndSet(nullptr, 0, 1));
    CHECK(Intf64_type(nullptr) == 0);
    CHECK(Intf64_length(nullptr) == 0);
    Intf64_free(nullptr);

    Intf64_free(p);
    Intf64_free(c);
    Intf64_free(r);
    Intf64_free(s);
    Intf64_free(arr);
    Intf64_shutdown();

    if (g_failures == 0) {
        printf("int_f64_test: all assertions held\n");
        return 0;
    }
    printf("int_f64_test: %d FAILURES\n", g_failures);
    return 1;
}
