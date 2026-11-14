// vexspoke/test/primitive/f32_test.c — the Float class _test.

#include <math.h>
#include <stdio.h>

#include "primitive/vex-f32.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Float values including signed zero, constructors, CAS, arrays, and null safety.
int main() {
    CHECK(Float_init());

    any p = Float_alloc();
    CHECK(p != nullptr);
    CHECK(Float_get(p) == 0.0f);
    CHECK(Float_type(p) == ID_FLOAT);
    CHECK(Float_length(p) == 4);

    const f32 values[] = { 0.0f, 1.0f, -1.0f, 3.5f, -0.0f };
    for (bytes i = 0; i < sizeof values / sizeof values[0]; i++) {
        Float_set(p, values[i]);
        CHECK(Float_get(p) == values[i]);
    }

    any q = Float_allocWithValue(2.5f);
    CHECK(q != nullptr && Float_get(q) == 2.5f);
    any r = Float(-1.25f);
    CHECK(r != nullptr && Float_get(r) == -1.25f);
    any s = Float_0();
    CHECK(s != nullptr && Float_type(s) == ID_FLOAT);

    Float_set(p, 1.0f);
    CHECK(Float_compareAndSet(p, 1.0f, 9.0f));
    CHECK(Float_get(p) == 9.0f);
    CHECK(!Float_compareAndSet(p, 1.0f, 4.0f));
    CHECK(Float_get(p) == 9.0f);

    CHECK(Float_allocArray(0) == nullptr);
    any arr = Float_array(8);
    CHECK(arr != nullptr);
    CHECK((Float_type(arr) & MASK_CLASS) == ID_FLOAT);
    CHECK(Float_length(arr) >= 8);

    CHECK(Float_get(nullptr) == 0.0f);
    Float_set(nullptr, 1.0f);
    CHECK(!Float_compareAndSet(nullptr, 0.0f, 1.0f));
    CHECK(Float_type(nullptr) == 0);
    CHECK(Float_length(nullptr) == 0);
    Float_free(nullptr);

    Float_free(p);
    Float_free(q);
    Float_free(r);
    Float_free(s);
    Float_free(arr);
    Float_shutdown();

    if (g_failures == 0) {
        printf("f32_test: all assertions held\n");
        return 0;
    }
    printf("f32_test: %d FAILURES\n", g_failures);
    return 1;
}
