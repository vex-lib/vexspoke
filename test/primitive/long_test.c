// vexspoke/test/primitive/long_test.c — the Long class _test.

#include <stdint.h>
#include <stdio.h>

#include "primitive/long.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Long signed boundaries, constructors, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Long_init());

    void *p = Long_alloc();
    CHECK(p != nullptr);
    CHECK(Long_get(p) == 0);
    CHECK(Long_type(p) == ID_LONG);
    CHECK(Long_length(p) == 8);

    const int64_t values[] = { 0, 1, -1, INT64_MAX, INT64_MIN };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        Long_set(p, values[i]);
        CHECK(Long_get(p) == values[i]);
    }

    void *q = Long_allocWithValue(42);
    CHECK(q != nullptr && Long_get(q) == 42);
    void *r = Long(-7);
    CHECK(r != nullptr && Long_get(r) == -7);
    void *s = Long_0();
    CHECK(s != nullptr && Long_type(s) == ID_LONG);

    CHECK(Long_compareAndSet(p, Long_get(p), 999));
    CHECK(Long_get(p) == 999);
    CHECK(!Long_compareAndSet(p, 12345, 5));
    CHECK(Long_get(p) == 999);

    CHECK(Long_allocArray(0) == nullptr);
    void *arr = Long_array(8);
    CHECK(arr != nullptr);
    CHECK((Long_type(arr) & MASK_CLASS) == ID_LONG);
    CHECK(Long_length(arr) >= 8);

    CHECK(Long_get(nullptr) == 0);
    Long_set(nullptr, 5);
    CHECK(!Long_compareAndSet(nullptr, 0, 1));
    CHECK(Long_type(nullptr) == 0);
    CHECK(Long_length(nullptr) == 0);
    Long_free(nullptr);

    Long_free(p);
    Long_free(q);
    Long_free(r);
    Long_free(s);
    Long_free(arr);
    Long_shutdown();

    if (g_failures == 0) {
        printf("long_test: all assertions held\n");
        return 0;
    }
    printf("long_test: %d FAILURES\n", g_failures);
    return 1;
}
