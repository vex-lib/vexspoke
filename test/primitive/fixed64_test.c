// vexspoke/test/primitive/fixed64_test.c — the Fixed64 class _test.

#include <stdint.h>
#include <stdio.h>

#include "primitive/fixed64.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Fixed64 boundary values, constructors, CAS, arrays, and null safety.
int main(void) {
    CHECK(Fixed64_init());

    void *p = Fixed64_alloc();
    CHECK(p != nullptr);
    CHECK(Fixed64_get(p) == 0);
    CHECK(Fixed64_type(p) == ID_FIXED64);
    CHECK(Fixed64_length(p) == 8);

    const int64_t values[] = { 0, 1, -1, 4294967296LL, 4294967295LL, INT64_MIN };
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        Fixed64_set(p, values[i]);
        CHECK(Fixed64_get(p) == values[i]);
    }

    void *q = Fixed64_allocWithValue(4294967296LL);
    CHECK(q != nullptr && Fixed64_get(q) == 4294967296LL);
    void *r = Fixed64(-4294967296LL);
    CHECK(r != nullptr && Fixed64_get(r) == -4294967296LL);
    void *s = Fixed64_0();
    CHECK(s != nullptr && Fixed64_type(s) == ID_FIXED64);

    CHECK(Fixed64_compareAndSet(p, Fixed64_get(p), 999));
    CHECK(Fixed64_get(p) == 999);
    CHECK(!Fixed64_compareAndSet(p, 12345, 5));
    CHECK(Fixed64_get(p) == 999);

    CHECK(Fixed64_allocArray(0) == nullptr);
    void *arr = Fixed64_array(8);
    CHECK(arr != nullptr);
    CHECK((Fixed64_type(arr) & MASK_CLASS) == ID_FIXED64);
    CHECK(Fixed64_length(arr) >= 8);

    CHECK(Fixed64_get(nullptr) == 0);
    Fixed64_set(nullptr, 5);
    CHECK(!Fixed64_compareAndSet(nullptr, 0, 1));
    CHECK(Fixed64_type(nullptr) == 0);
    CHECK(Fixed64_length(nullptr) == 0);
    Fixed64_free(nullptr);

    Fixed64_free(p);
    Fixed64_free(q);
    Fixed64_free(r);
    Fixed64_free(s);
    Fixed64_free(arr);
    Fixed64_shutdown();

    if (g_failures == 0) {
        printf("fixed64_test: all assertions held\n");
        return 0;
    }
    printf("fixed64_test: %d FAILURES\n", g_failures);
    return 1;
}
