// vexspoke/test/primitive/f64_test.c — the d64 class _test.

#include <math.h>
#include <stdio.h>

#include "primitive/d64.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises d64 values including signed zero, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(f64_init());

    void *p = f64_alloc();
    CHECK(p != nullptr);
    CHECK(f64_get(p) == 0.0);
    CHECK(f64_type(p) == ID_f64);
    CHECK(f64_length(p) == 8);

    const d64 values[] = { 0.0, 1.0, -1.0, 3.14159265358979, -0.0 };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        f64_set(p, values[i]);
        CHECK(f64_get(p) == values[i]);
    }

    void *q = f64_allocWithValue(2.5);
    CHECK(q != nullptr && f64_get(q) == 2.5);
    void *r = d64(-1.25);
    CHECK(r != nullptr && f64_get(r) == -1.25);
    void *s = f64_0();
    CHECK(s != nullptr && f64_type(s) == ID_f64);

    f64_set(p, 1.0);
    CHECK(f64_compareAndSet(p, 1.0, 9.0));
    CHECK(f64_get(p) == 9.0);
    CHECK(!f64_compareAndSet(p, 1.0, 4.0));
    CHECK(f64_get(p) == 9.0);

    CHECK(f64_allocArray(0) == nullptr);
    void *arr = f64_array(8);
    CHECK(arr != nullptr);
    CHECK((f64_type(arr) & MASK_CLASS) == ID_f64);
    CHECK(f64_length(arr) >= 8);

    CHECK(f64_get(nullptr) == 0.0);
    f64_set(nullptr, 1.0);
    CHECK(!f64_compareAndSet(nullptr, 0.0, 1.0));
    CHECK(f64_type(nullptr) == 0);
    CHECK(f64_length(nullptr) == 0);
    f64_free(nullptr);

    f64_free(p);
    f64_free(q);
    f64_free(r);
    f64_free(s);
    f64_free(arr);
    f64_shutdown();

    if (g_failures == 0) {
        printf("f64_test: all assertions held\n");
        return 0;
    }
    printf("f64_test: %d FAILURES\n", g_failures);
    return 1;
}
