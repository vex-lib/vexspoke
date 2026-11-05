// vexspoke/test/primitive/double_test.c — the Double class _test.

#include <math.h>
#include <stdio.h>

#include "primitive/double.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Double values including signed zero, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Double_init());

    void *p = Double_alloc();
    CHECK(p != nullptr);
    CHECK(Double_get(p) == 0.0);
    CHECK(Double_type(p) == ID_DOUBLE);
    CHECK(Double_length(p) == 8);

    const double values[] = { 0.0, 1.0, -1.0, 3.14159265358979, -0.0 };
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        Double_set(p, values[i]);
        CHECK(Double_get(p) == values[i]);
    }

    void *q = Double_allocWithValue(2.5);
    CHECK(q != nullptr && Double_get(q) == 2.5);
    void *r = Double(-1.25);
    CHECK(r != nullptr && Double_get(r) == -1.25);
    void *s = Double_0();
    CHECK(s != nullptr && Double_type(s) == ID_DOUBLE);

    Double_set(p, 1.0);
    CHECK(Double_compareAndSet(p, 1.0, 9.0));
    CHECK(Double_get(p) == 9.0);
    CHECK(!Double_compareAndSet(p, 1.0, 4.0));
    CHECK(Double_get(p) == 9.0);

    CHECK(Double_allocArray(0) == nullptr);
    void *arr = Double_array(8);
    CHECK(arr != nullptr);
    CHECK((Double_type(arr) & MASK_CLASS) == ID_DOUBLE);
    CHECK(Double_length(arr) >= 8);

    CHECK(Double_get(nullptr) == 0.0);
    Double_set(nullptr, 1.0);
    CHECK(!Double_compareAndSet(nullptr, 0.0, 1.0));
    CHECK(Double_type(nullptr) == 0);
    CHECK(Double_length(nullptr) == 0);
    Double_free(nullptr);

    Double_free(p);
    Double_free(q);
    Double_free(r);
    Double_free(s);
    Double_free(arr);
    Double_shutdown();

    if (g_failures == 0) {
        printf("double_test: all assertions held\n");
        return 0;
    }
    printf("double_test: %d FAILURES\n", g_failures);
    return 1;
}
