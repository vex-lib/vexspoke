// vexspoke/test/primitive/long_f32_test.c — the LongFloat class _test.
//
// Composite: int64 (offset 0) + f32 (offset 8); the API reads/writes int64.

#include <stdint.h>
#include <stdio.h>

#include "primitive/long_f32.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises LongFloat's composite layout, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(LongFloat_init());

    void *p = LongFloat_alloc();
    CHECK(p != nullptr);
    CHECK(LongFloat_get(p) == 0);
    CHECK(LongFloat_type(p) == ID_LONG_FLOAT);
    CHECK(LongFloat_length(p) == 16);

    LongFloat_set(p, 0x0123456789ABCDEFLL);
    CHECK(LongFloat_get(p) == 0x0123456789ABCDEFLL);
    LongFloat_set(p, -1);
    CHECK(LongFloat_get(p) == -1);

    void *c = LongFloat_allocWithValues(7, 1.5f);
    CHECK(c != nullptr);
    CHECK(LongFloat_get(c) == 7); // v1 (int64) landed at offset 0

    void *r = LongFloat_2(4, 0.5f);
    CHECK(r != nullptr && LongFloat_get(r) == 4);
    void *s = LongFloat_0();
    CHECK(s != nullptr && LongFloat_type(s) == ID_LONG_FLOAT);

    LongFloat_set(p, 11);
    CHECK(LongFloat_compareAndSet(p, 11, 22));
    CHECK(LongFloat_get(p) == 22);
    CHECK(!LongFloat_compareAndSet(p, 11, 33));
    CHECK(LongFloat_get(p) == 22);

    CHECK(LongFloat_allocArray(0) == nullptr);
    void *arr = LongFloat_array(4);
    CHECK(arr != nullptr);
    CHECK((LongFloat_type(arr) & MASK_CLASS) == ID_LONG_FLOAT);
    CHECK(LongFloat_length(arr) >= 4);

    CHECK(LongFloat_get(nullptr) == 0);
    LongFloat_set(nullptr, 5);
    CHECK(!LongFloat_compareAndSet(nullptr, 0, 1));
    CHECK(LongFloat_type(nullptr) == 0);
    CHECK(LongFloat_length(nullptr) == 0);
    LongFloat_free(nullptr);

    LongFloat_free(p);
    LongFloat_free(c);
    LongFloat_free(r);
    LongFloat_free(s);
    LongFloat_free(arr);
    LongFloat_shutdown();

    if (g_failures == 0) {
        printf("long_f32_test: all assertions held\n");
        return 0;
    }
    printf("long_f32_test: %d FAILURES\n", g_failures);
    return 1;
}
