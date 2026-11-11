// vexspoke/test/primitive/short_test.c — the Short class _test.

#include <stdint.h>
#include <stdio.h>

#include "primitive/short.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Short signed boundaries, constructors, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Short_init());

    void *p = Short_alloc();
    CHECK(p != nullptr);
    CHECK(Short_get(p) == 0);
    CHECK(Short_type(p) == ID_SHORT);
    CHECK(Short_length(p) == 2);

    const  values[] = { 0, 1, -1, 32767, -32768 };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        Short_set(p, values[i]);
        CHECK(Short_get(p) == values[i]);
    }

    void *q = Short_allocWithValue(42);
    CHECK(q != nullptr && Short_get(q) == 42);
    void *r = Short(-7);
    CHECK(r != nullptr && Short_get(r) == -7);
    void *s = Short_0();
    CHECK(s != nullptr && Short_type(s) == ID_SHORT);

    CHECK(Short_compareAndSet(p, Short_get(p), 999));
    CHECK(Short_get(p) == 999);
    CHECK(!Short_compareAndSet(p, 123, 5));
    CHECK(Short_get(p) == 999);

    CHECK(Short_allocArray(0) == nullptr);
    void *arr = Short_array(8);
    CHECK(arr != nullptr);
    CHECK((Short_type(arr) & MASK_CLASS) == ID_SHORT);
    CHECK(Short_length(arr) >= 8);

    CHECK(Short_get(nullptr) == 0);
    Short_set(nullptr, 5);
    CHECK(!Short_compareAndSet(nullptr, 0, 1));
    CHECK(Short_type(nullptr) == 0);
    CHECK(Short_length(nullptr) == 0);
    Short_free(nullptr);

    Short_free(p);
    Short_free(q);
    Short_free(r);
    Short_free(s);
    Short_free(arr);
    Short_shutdown();

    if (g_failures == 0) {
        printf("short_test: all assertions held\n");
        return 0;
    }
    printf("short_test: %d FAILURES\n", g_failures);
    return 1;
}
