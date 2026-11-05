// vexspoke/test/primitive/int_test.c — the Int class _test.
//
// Proves primitive/int.c: BitPool-backed 32-bit payloads — init, alloc/allocArray,
// set/get across the int32 boundaries, compare-and-set, type/length identity
// (pool vs arena routing), the constructor macros, and null-safety.

#include <stdint.h>
#include <stdio.h>

#include "primitive/int.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Int boundaries, allocation forms, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Int_init());

    void *p = Int_alloc();
    CHECK(p != nullptr);
    CHECK(Int_get(p) == 0);
    CHECK(Int_type(p) == ID_INT);
    CHECK(Int_length(p) == 4);

    const int32_t values[] = { 0, 1, -1, 2147483647, (int32_t) (-2147483647 - 1) };
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
        Int_set(p, values[i]);
        CHECK(Int_get(p) == values[i]);
    }

    void *q = Int_allocWithValue(42);
    CHECK(q != nullptr && Int_get(q) == 42);
    void *r = Int(-7); // dispatch macro -> Int_1
    CHECK(r != nullptr && Int_get(r) == -7);
    void *s = Int_0();
    CHECK(s != nullptr && Int_type(s) == ID_INT);

    CHECK(Int_compareAndSet(p, Int_get(p), 999));
    CHECK(Int_get(p) == 999);
    CHECK(!Int_compareAndSet(p, 12345, 5));
    CHECK(Int_get(p) == 999);

    CHECK(Int_allocArray(0) == nullptr);
    void *arr = Int_array(8);
    CHECK(arr != nullptr);
    CHECK((Int_type(arr) & MASK_CLASS) == ID_INT);
    CHECK(Int_length(arr) >= 8);

    CHECK(Int_get(nullptr) == 0);
    Int_set(nullptr, 5);
    CHECK(!Int_compareAndSet(nullptr, 0, 1));
    CHECK(Int_type(nullptr) == 0);
    CHECK(Int_length(nullptr) == 0);
    Int_free(nullptr);

    Int_free(p);
    Int_free(q);
    Int_free(r);
    Int_free(s);
    Int_free(arr);
    Int_shutdown();

    if (g_failures == 0) {
        printf("int_test: all assertions held\n");
        return 0;
    }
    printf("int_test: %d FAILURES\n", g_failures);
    return 1;
}
