// vexspoke/test/primitive/pack_test.c — the Pack class _test.
//
// pack.h is stateless inline bit-packing (pack.h ships the helpers; pack.c
// carries no state). Proves byte/short/int pack+unpack round trips, including
// the sign-carrying negative cases.

#include "vex.h"
#include <stdio.h>

#include "primitive/pack.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Verifies signed and boundary round trips for byte, short, and int packing helpers.
int main() {
    // Byte pair -> short.
    CHECK(Pack_packByte(1, 2) == 0x0102);
    CHECK(Pack_unpackByte1(Pack_packByte(1, 2)) == 1);
    CHECK(Pack_unpackByte2(Pack_packByte(1, 2)) == 2);
    CHECK(Pack_unpackByte1(Pack_packByte(-1, -2)) == -1);
    CHECK(Pack_unpackByte2(Pack_packByte(-1, -2)) == -2);
    CHECK(Pack_unpackByte2(Pack_packByte(0x7F, 0x80)) == (i8) 0x80);

    // Short pair -> int.
    CHECK(Pack_packShort(1, 2) == 0x00010002);
    CHECK(Pack_unpackShort1(Pack_packShort(3, 4)) == 3);
    CHECK(Pack_unpackShort2(Pack_packShort(3, 4)) == 4);
    CHECK(Pack_unpackShort1(Pack_packShort(-5, -6)) == -5);
    CHECK(Pack_unpackShort2(Pack_packShort(-5, -6)) == -6);

    // Int pair -> long.
    CHECK(Pack_unpackInt1(Pack_packInt(5, 6)) == 5);
    CHECK(Pack_unpackInt2(Pack_packInt(5, 6)) == 6);
    CHECK(Pack_unpackInt1(Pack_packInt(-7, -8)) == -7);
    CHECK(Pack_unpackInt2(Pack_packInt(-7, -8)) == -8);
    CHECK(Pack_unpackInt1(Pack_packInt(2147483647, -2147483647 - 1)) == 2147483647);

    if (g_failures == 0) {
        printf("pack_test: all assertions held\n");
        return 0;
    }
    printf("pack_test: %d FAILURES\n", g_failures);
    return 1;
}
