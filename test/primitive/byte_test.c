// vexspoke/test/primitive/byte_test.c — the Byte class _test.

#include <stdint.h>
#include <stdio.h>

#include "primitive/byte.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises signed Byte values, constructors, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Byte_init());

    any p = Byte_alloc();
    CHECK(p != nullptr);
    CHECK(Byte_get(p) == 0);
    CHECK(Byte_type(p) == ID_BYTE);
    CHECK(Byte_length(p) == 1);

    const  values[] = { 0, 1, -1, 127, -128 };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        Byte_set(p, values[i]);
        CHECK(Byte_get(p) == values[i]);
    }

    any q = Byte_allocWithValue(42);
    CHECK(q != nullptr && Byte_get(q) == 42);
    any r = Byte(-7);
    CHECK(r != nullptr && Byte_get(r) == -7);
    any s = Byte_0();
    CHECK(s != nullptr && Byte_type(s) == ID_BYTE);

    CHECK(Byte_compareAndSet(p, Byte_get(p), 99));
    CHECK(Byte_get(p) == 99);
    CHECK(!Byte_compareAndSet(p, 12, 5));
    CHECK(Byte_get(p) == 99);

    CHECK(Byte_allocArray(0) == nullptr);
    any arr = Byte_array(8);
    CHECK(arr != nullptr);
    CHECK((Byte_type(arr) & MASK_CLASS) == ID_BYTE);
    CHECK(Byte_length(arr) >= 8);

    CHECK(Byte_get(nullptr) == 0);
    Byte_set(nullptr, 5);
    CHECK(!Byte_compareAndSet(nullptr, 0, 1));
    CHECK(Byte_type(nullptr) == 0);
    CHECK(Byte_length(nullptr) == 0);
    Byte_free(nullptr);

    Byte_free(p);
    Byte_free(q);
    Byte_free(r);
    Byte_free(s);
    Byte_free(arr);
    Byte_shutdown();

    if (g_failures == 0) {
        printf("byte_test: all assertions held\n");
        return 0;
    }
    printf("byte_test: %d FAILURES\n", g_failures);
    return 1;
}
