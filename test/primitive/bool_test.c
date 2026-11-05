// vexspoke/test/primitive/bool_test.c — the Bool class _test.

#include <stdbool.h>
#include <stdio.h>

#include "primitive/bool.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Bool initialization, scalar/array allocation, CAS, metadata, and null safety.
int main(void) {
    CHECK(Bool_init());

    void *p = Bool_alloc();
    CHECK(p != nullptr);
    CHECK(Bool_get(p) == false);
    CHECK(Bool_type(p) == ID_BOOL);
    CHECK(Bool_length(p) == 1);

    Bool_set(p, true);
    CHECK(Bool_get(p) == true);
    Bool_set(p, false);
    CHECK(Bool_get(p) == false);

    void *q = Bool_allocWithValue(true);
    CHECK(q != nullptr && Bool_get(q) == true);
    void *r = Bool(true);
    CHECK(r != nullptr && Bool_get(r) == true);
    void *s = Bool_0();
    CHECK(s != nullptr && Bool_type(s) == ID_BOOL);

    Bool_set(p, false);
    CHECK(Bool_compareAndSet(p, false, true));
    CHECK(Bool_get(p) == true);
    CHECK(!Bool_compareAndSet(p, false, false));
    CHECK(Bool_get(p) == true);

    CHECK(Bool_allocArray(0) == nullptr);
    void *arr = Bool_array(8);
    CHECK(arr != nullptr);
    CHECK((Bool_type(arr) & MASK_CLASS) == ID_BOOL);
    CHECK(Bool_length(arr) >= 8);

    CHECK(Bool_get(nullptr) == false);
    Bool_set(nullptr, true);
    CHECK(!Bool_compareAndSet(nullptr, false, true));
    CHECK(Bool_type(nullptr) == 0);
    CHECK(Bool_length(nullptr) == 0);
    Bool_free(nullptr);

    Bool_free(p);
    Bool_free(q);
    Bool_free(r);
    Bool_free(s);
    Bool_free(arr);
    Bool_shutdown();

    if (g_failures == 0) {
        printf("bool_test: all assertions held\n");
        return 0;
    }
    printf("bool_test: %d FAILURES\n", g_failures);
    return 1;
}
