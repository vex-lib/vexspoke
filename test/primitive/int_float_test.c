// vexspoke/test/primitive/int_f32_test.c — the IntFloat class _test.
//
// The composite payload is int32 (offset 0) + f32 (offset 4); the public API
// reads/writes it as one int64, so the assertions pin both halves.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "primitive/int_f32.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises IntFloat's packed bit layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(IntFloat_init());

    void *p = IntFloat_alloc();
    CHECK(p != nullptr);
    CHECK(IntFloat_get(p) == 0);
    CHECK(IntFloat_type(p) == ID_INT_FLOAT);
    CHECK(IntFloat_length(p) == 8);

    IntFloat_set(p, 0x0123456789ABCDEFLL);
    CHECK(IntFloat_get(p) == 0x0123456789ABCDEFLL);
    IntFloat_set(p, -1);
    CHECK(IntFloat_get(p) == -1);

    void *c = IntFloat_allocWithValues(7, 1.5f);
    CHECK(c != nullptr);
    u32 fbits = 0;
    f32 v2 = 1.5f;
    memcpy(&fbits, &v2, sizeof fbits);
    int64_t expected = ((int64_t) fbits << 32) | (u32) 7;
    CHECK(IntFloat_get(c) == expected);

    void *r = IntFloat_2(3, 0.5f);
    CHECK(r != nullptr);
    CHECK((IntFloat_get(r) & 0xFFFFFFFFLL) == 3);
    void *s = IntFloat_0();
    CHECK(s != nullptr && IntFloat_type(s) == ID_INT_FLOAT);

    IntFloat_set(p, 11);
    CHECK(IntFloat_compareAndSet(p, 11, 22));
    CHECK(IntFloat_get(p) == 22);
    CHECK(!IntFloat_compareAndSet(p, 11, 33));
    CHECK(IntFloat_get(p) == 22);

    CHECK(IntFloat_allocArray(0) == nullptr);
    void *arr = IntFloat_array(4);
    CHECK(arr != nullptr);
    CHECK((IntFloat_type(arr) & MASK_CLASS) == ID_INT_FLOAT);
    CHECK(IntFloat_length(arr) >= 4);

    CHECK(IntFloat_get(nullptr) == 0);
    IntFloat_set(nullptr, 5);
    CHECK(!IntFloat_compareAndSet(nullptr, 0, 1));
    CHECK(IntFloat_type(nullptr) == 0);
    CHECK(IntFloat_length(nullptr) == 0);
    IntFloat_free(nullptr);

    IntFloat_free(p);
    IntFloat_free(c);
    IntFloat_free(r);
    IntFloat_free(s);
    IntFloat_free(arr);
    IntFloat_shutdown();

    if (g_failures == 0) {
        printf("int_f32_test: all assertions held\n");
        return 0;
    }
    printf("int_f32_test: %d FAILURES\n", g_failures);
    return 1;
}
