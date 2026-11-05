// vexspoke/test/oop/stride_test.c — the Stride class _test.
//
// Proves oop/stride.c: the class-id -> byte-width table. Known ids answer their
// exact width; a full TYPE_ id is masked to its class field; unknown ids answer
// the pointer-sized default (8). Pure, stateless, headless.

#include <stdint.h>
#include <stdio.h>

#include "oop/stride.h"
#include "oop/type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks class-to-byte-width mappings and full type-id class masking.
int main(void) {
    // Scalar widths.
    CHECK(Stride_get(ID_BYTE) == 1);
    CHECK(Stride_get(ID_SHORT) == 2);
    CHECK(Stride_get(ID_INT) == 4);
    CHECK(Stride_get(ID_FLOAT) == 4);
    CHECK(Stride_get(ID_LONG) == 8);
    CHECK(Stride_get(ID_DOUBLE) == 8);
    CHECK(Stride_get(ID_INT_DOUBLE) == 16);
    CHECK(Stride_get(ID_LONG_DOUBLE) == 16);

    // Geometry + outliers.
    CHECK(Stride_get(ID_VEC2) == 8);
    CHECK(Stride_get(ID_VEC3) == 12);
    CHECK(Stride_get(ID_VEC4) == 16);
    CHECK(Stride_get(ID_MAT4) == 64);
    CHECK(Stride_get(ID_VARIABLE) == 40);

    // Unknown class ids answer the pointer-sized default.
    CHECK(Stride_get(0x7FFFu) == 8);

    // A full TYPE_ id is masked to its class field before lookup.
    CHECK(Stride_get((uint32_t) TYPE_VEC3_SINGLETON) == 12);
    CHECK(Stride_get((uint32_t) TYPE_VEC4_SINGLETON) == 16);

    if (g_failures == 0) {
        printf("stride_test: all assertions held\n");
        return 0;
    }
    printf("stride_test: %d FAILURES\n", g_failures);
    return 1;
}
