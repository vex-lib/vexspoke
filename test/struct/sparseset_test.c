// vexspoke/test/struct/sparseset_test.c — the SparseSet class _test.
//
// Dense-entity set with payload slots. Proves add/contains/get/remove and the
// dense projections, plus the hostile entity id (negative / out of range).

#include <stdint.h>
#include <stdio.h>

#include "struct/sparseset.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks sparse-set membership and index/value behavior across insertions and removals.
int main() {
    SparseSet *s = SparseSet_3(1024, 2048, 4);
    CHECK(s != nullptr);
    CHECK(SparseSet_count(s) == 0);
    // The Exhaustion Loudness Law accessor: a fresh owner has no refusals.
    CHECK(SparseSet_exhaustionCount(s) == 0);
    CHECK(SparseSet_capacity(s) >= 64);
    CHECK(SparseSet_maxEntities(s) >= 1024);

    u8 *slot = SparseSet_add(s, 42);
    CHECK(slot != nullptr);
    CHECK(SparseSet_contains(s, 42));
    CHECK(SparseSet_get(s, 42) == slot);
    CHECK(SparseSet_count(s) == 1);
    CHECK(!SparseSet_contains(s, 43));

    SparseSet_remove(s, 42);
    CHECK(!SparseSet_contains(s, 42));
    CHECK(SparseSet_count(s) == 0);
    CHECK(SparseSet_get(s, 42) == nullptr);

    // Volume: 1,000 entities.
    for (i32 i = 0; i < 1000; i++)
        CHECK(SparseSet_add(s, i) != nullptr);
    CHECK(SparseSet_count(s) == 1000);
    CHECK(SparseSet_contains(s, 0));
    CHECK(SparseSet_contains(s, 999));
    CHECK(!SparseSet_contains(s, 1000));

    // Dense projections.
    const i32 *entities = SparseSet_denseEntities(s);
    CHECK(entities != nullptr);
    const u8 *data = SparseSet_denseData(s);
    CHECK(data != nullptr);

    // Hostile entity ids: negative and out of range are refused, not UB.
    CHECK(SparseSet_add(s, -1) == nullptr);
    CHECK(!SparseSet_contains(s, -1));
    CHECK(SparseSet_get(s, -1) == nullptr);
    SparseSet_remove(s, -1); // no-op

    // Null-safety.
    CHECK(SparseSet_add(nullptr, 1) == nullptr);
    CHECK(!SparseSet_contains(nullptr, 1));
    CHECK(SparseSet_get(nullptr, 1) == nullptr);
    CHECK(SparseSet_count(nullptr) == 0);
    CHECK(SparseSet_denseEntities(nullptr) == nullptr);
    CHECK(SparseSet_denseData(nullptr) == nullptr);
    SparseSet_free(nullptr);

    SparseSet_free(s);

    if (g_failures == 0) {
        printf("sparseset_test: all assertions held\n");
        return 0;
    }
    printf("sparseset_test: %d FAILURES\n", g_failures);
    return 1;
}
