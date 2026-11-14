// vexspoke/test/algo/spatial_test.c — owner tests for Spatial indexing helpers.

#include <stdint.h>
#include <stdio.h>

#include "algo/spatial.h"

static int g_failures = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition);      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

static unsigned countBits(u64 value) {
    unsigned count = 0u;
    while (value != 0u) {
        count += (unsigned) (value & 1u);
        value >>= 1u;
    }
    return count;
}

int main() {
    CHECK(Spatial_morton3D(0u, 0u, 0u) == 0u);
    u64 xBit = Spatial_morton3D(1u, 0u, 0u);
    u64 yBit = Spatial_morton3D(0u, 1u, 0u);
    u64 zBit = Spatial_morton3D(0u, 0u, 1u);
    CHECK(countBits(xBit) == 1u && countBits(yBit) == 1u && countBits(zBit) == 1u);
    CHECK(xBit != yBit && yBit != zBit && xBit != zBit);
    CHECK(Spatial_morton3D(0x1FFFFFu, 0x1FFFFFu, 0x1FFFFFu)
          == 0x7FFFFFFFFFFFFFFFull);
    CHECK(Spatial_morton3D(1u << 21u, 0u, 0u) == 0u);
    CHECK(Spatial_morton3D(0u, 1u << 31u, 0u) == 0u);
    CHECK(Spatial_morton3D(1u, 1u, 1u) != Spatial_morton3D(2u, 0u, 0u));
    if (g_failures == 0) {
        puts("spatial_test: all assertions held");
        return 0;
    }
    printf("spatial_test: %d failures\n", g_failures);
    return 1;
}
