// vexspoke/test/primitive/long_double_test.c — the LongDouble class _test.
//
// Composite: int64 (offset 0) + double (offset 8); the API reads/writes int64.

#include <stdint.h>
#include <stdio.h>

#include "primitive/long_double.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises LongDouble's composite layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(LongDouble_init());

    void *p = LongDouble_alloc();
    CHECK(p != nullptr);
    CHECK(LongDouble_get(p) == 0);
    CHECK(LongDouble_type(p) == ID_LONG_DOUBLE);
    CHECK(LongDouble_length(p) == 16);

    LongDouble_set(p, 0x0123456789ABCDEFLL);
    CHECK(LongDouble_get(p) == 0x0123456789ABCDEFLL);
    LongDouble_set(p, -1);
    CHECK(LongDouble_get(p) == -1);

    void *c = LongDouble_allocWithValues(7, 2.5);
    CHECK(c != nullptr);
    CHECK(LongDouble_get(c) == 7); // v1 (int64) landed at offset 0

    void *r = LongDouble_2(4, 0.5);
    CHECK(r != nullptr && LongDouble_get(r) == 4);
    void *s = LongDouble_0();
    CHECK(s != nullptr && LongDouble_type(s) == ID_LONG_DOUBLE);

    LongDouble_set(p, 11);
    CHECK(LongDouble_compareAndSet(p, 11, 22));
    CHECK(LongDouble_get(p) == 22);
    CHECK(!LongDouble_compareAndSet(p, 11, 33));
    CHECK(LongDouble_get(p) == 22);

    CHECK(LongDouble_allocArray(0) == nullptr);
    void *arr = LongDouble_array(4);
    CHECK(arr != nullptr);
    CHECK((LongDouble_type(arr) & MASK_CLASS) == ID_LONG_DOUBLE);
    CHECK(LongDouble_length(arr) >= 4);

    CHECK(LongDouble_get(nullptr) == 0);
    LongDouble_set(nullptr, 5);
    CHECK(!LongDouble_compareAndSet(nullptr, 0, 1));
    CHECK(LongDouble_type(nullptr) == 0);
    CHECK(LongDouble_length(nullptr) == 0);
    LongDouble_free(nullptr);

    LongDouble_free(p);
    LongDouble_free(c);
    LongDouble_free(r);
    LongDouble_free(s);
    LongDouble_free(arr);
    LongDouble_shutdown();

    if (g_failures == 0) {
        printf("long_double_test: all assertions held\n");
        return 0;
    }
    printf("long_double_test: %d FAILURES\n", g_failures);
    return 1;
}
