// vexspoke/test/primitive/fixed32_test.c — the Fixed32 class _test.

#include <stdint.h>
#include <stdio.h>

#include "primitive/fixed32.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Fixed32 boundary values, constructors, CAS, arrays, and null safety.
int main() {
    CHECK(Fixed32_init());

    any p = Fixed32_alloc();
    CHECK(p != nullptr);
    CHECK(Fixed32_get(p) == 0);
    CHECK(Fixed32_type(p) == ID_FIXED32);
    CHECK(Fixed32_length(p) == 4);

    const i32 values[] = { 0, 1, -1, 65536, 65535, (i32) (-2147483647 - 1) };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        Fixed32_set(p, values[i]);
        CHECK(Fixed32_get(p) == values[i]);
    }

    any q = Fixed32_allocWithValue(65536);
    CHECK(q != nullptr && Fixed32_get(q) == 65536);
    any r = Fixed32(-65536);
    CHECK(r != nullptr && Fixed32_get(r) == -65536);
    any s = Fixed32_0();
    CHECK(s != nullptr && Fixed32_type(s) == ID_FIXED32);

    CHECK(Fixed32_compareAndSet(p, Fixed32_get(p), 999));
    CHECK(Fixed32_get(p) == 999);
    CHECK(!Fixed32_compareAndSet(p, 12345, 5));
    CHECK(Fixed32_get(p) == 999);

    CHECK(Fixed32_allocArray(0) == nullptr);
    any arr = Fixed32_array(8);
    CHECK(arr != nullptr);
    CHECK((Fixed32_type(arr) & MASK_CLASS) == ID_FIXED32);
    CHECK(Fixed32_length(arr) >= 8);

    CHECK(Fixed32_get(nullptr) == 0);
    Fixed32_set(nullptr, 5);
    CHECK(!Fixed32_compareAndSet(nullptr, 0, 1));
    CHECK(Fixed32_type(nullptr) == 0);
    CHECK(Fixed32_length(nullptr) == 0);
    Fixed32_free(nullptr);

    Fixed32_free(p);
    Fixed32_free(q);
    Fixed32_free(r);
    Fixed32_free(s);
    Fixed32_free(arr);
    Fixed32_shutdown();

    if (g_failures == 0) {
        printf("fixed32_test: all assertions held\n");
        return 0;
    }
    printf("fixed32_test: %d FAILURES\n", g_failures);
    return 1;
}
