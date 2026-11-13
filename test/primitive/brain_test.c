// vexspoke/test/primitive/brain_test.c — the Brain class _test.
//
// Brain is the bf3216 primitive: a uint16 payload plus f32<->bf3216
// conversion. 3.5 is exactly representable in bf3216, so those round trips
// are exact; other values are checked within the bf3216 epsilon.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "primitive/brain.h"
#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Exercises Brain storage, bf3216 conversions, CAS, arrays, metadata, and null safety.
int main(void) {
    CHECK(Brain_init());

    any p = Brain_alloc();
    CHECK(p != nullptr);
    CHECK(Brain_get(p) == 0);
    CHECK(Brain_type(p) == ID_BRAIN);
    CHECK(Brain_length(p) == 2);

    const u16 raw[] = { 0, 1, 0x3F80, 0xFFFF };
    for (bytes i = 0; i < sizeof raw / sizeof raw[0]; i++) {
        Brain_set(p, raw[i]);
        CHECK(Brain_get(p) == raw[i]);
    }

    // Exact bf3216 value.
    u16 bf35 = Brain_f32ToBFloat16(3.5f);
    CHECK(fabsf(Brain_bFloat16ToFloat(bf35) - 3.5f) < 1e-6f);

    // getFloat / setFloat round trip through the encoding.
    Brain_setFloat(p, 3.5f);
    CHECK(Brain_get(p) == bf35);
    CHECK(fabsf(Brain_getFloat(p) - 3.5f) < 1e-6f);

    // A non-exact value stays within the bf3216 epsilon (2^-8 relative).
    Brain_setFloat(p, 100.0f);
    CHECK(fabsf(Brain_getFloat(p) - 100.0f) < 1.0f);

    any q = Brain_allocWithValue(bf35);
    CHECK(q != nullptr && Brain_get(q) == bf35);
    any r = Brain(bf35);
    CHECK(r != nullptr && Brain_get(r) == bf35);
    any s = Brain_0();
    CHECK(s != nullptr && Brain_type(s) == ID_BRAIN);

    Brain_set(p, 1);
    CHECK(Brain_compareAndSet(p, 1, 2));
    CHECK(Brain_get(p) == 2);
    CHECK(!Brain_compareAndSet(p, 1, 3));
    CHECK(Brain_get(p) == 2);

    CHECK(Brain_allocArray(0) == nullptr);
    any arr = Brain_array(8);
    CHECK(arr != nullptr);
    CHECK((Brain_type(arr) & MASK_CLASS) == ID_BRAIN);
    CHECK(Brain_length(arr) >= 8);

    CHECK(Brain_get(nullptr) == 0);
    CHECK(Brain_getFloat(nullptr) == 0.0f);
    Brain_set(nullptr, 1);
    Brain_setFloat(nullptr, 1.0f);
    CHECK(!Brain_compareAndSet(nullptr, 0, 1));
    CHECK(Brain_type(nullptr) == 0);
    CHECK(Brain_length(nullptr) == 0);
    Brain_free(nullptr);

    Brain_free(p);
    Brain_free(q);
    Brain_free(r);
    Brain_free(s);
    Brain_free(arr);
    Brain_shutdown();

    if (g_failures == 0) {
        printf("brain_test: all assertions held\n");
        return 0;
    }
    printf("brain_test: %d FAILURES\n", g_failures);
    return 1;
}
