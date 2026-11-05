// vexspoke/test/primitive/int_double_test.c — the IntDouble class _test.
//
// Composite: int32 (offset 0) + double (offset 4); the API reads/writes int64.

#include <stdint.h>
#include <stdio.h>

#include "primitive/int_double.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises IntDouble's packed two-field layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(IntDouble_init());

    void *p = IntDouble_alloc();
    CHECK(p != nullptr);
    CHECK(IntDouble_get(p) == 0);
    CHECK(IntDouble_type(p) == ID_INT_DOUBLE);
    CHECK(IntDouble_length(p) == 16);

    IntDouble_set(p, 0x0123456789ABCDEFLL);
    CHECK(IntDouble_get(p) == 0x0123456789ABCDEFLL);
    IntDouble_set(p, -1);
    CHECK(IntDouble_get(p) == -1);

    void *c = IntDouble_allocWithValues(7, 2.5);
    CHECK(c != nullptr);
    CHECK((IntDouble_get(c) & 0xFFFFFFFFLL) == 7); // v1 landed at offset 0

    void *r = IntDouble_2(4, 0.5);
    CHECK(r != nullptr);
    CHECK((IntDouble_get(r) & 0xFFFFFFFFLL) == 4);
    void *s = IntDouble_0();
    CHECK(s != nullptr && IntDouble_type(s) == ID_INT_DOUBLE);

    IntDouble_set(p, 11);
    CHECK(IntDouble_compareAndSet(p, 11, 22));
    CHECK(IntDouble_get(p) == 22);
    CHECK(!IntDouble_compareAndSet(p, 11, 33));
    CHECK(IntDouble_get(p) == 22);

    CHECK(IntDouble_allocArray(0) == nullptr);
    void *arr = IntDouble_array(4);
    CHECK(arr != nullptr);
    CHECK((IntDouble_type(arr) & MASK_CLASS) == ID_INT_DOUBLE);
    CHECK(IntDouble_length(arr) >= 4);

    CHECK(IntDouble_get(nullptr) == 0);
    IntDouble_set(nullptr, 5);
    CHECK(!IntDouble_compareAndSet(nullptr, 0, 1));
    CHECK(IntDouble_type(nullptr) == 0);
    CHECK(IntDouble_length(nullptr) == 0);
    IntDouble_free(nullptr);

    IntDouble_free(p);
    IntDouble_free(c);
    IntDouble_free(r);
    IntDouble_free(s);
    IntDouble_free(arr);
    IntDouble_shutdown();

    if (g_failures == 0) {
        printf("int_double_test: all assertions held\n");
        return 0;
    }
    printf("int_double_test: %d FAILURES\n", g_failures);
    return 1;
}
