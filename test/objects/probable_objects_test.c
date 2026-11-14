// vexspoke/test/objects/probable_objects_test.c — the ProbableObjects class _test.
//
// The weighted pool: append choices, cumulative weights, a draw lands in the
// pool, capacity overflow is refused, and addProbable carries a Probable across.

#include "vex.h"
#include <stdio.h>
#include <string.h>
#if defined(VEX_PLATFORM_WINDOWS)
#include <io.h>
#define TEST_DUP _dup
#define TEST_DUP2 _dup2
#define TEST_CLOSE _close
#define TEST_FILENO _fileno
#else
#include <unistd.h>
#define TEST_DUP dup
#define TEST_DUP2 dup2
#define TEST_CLOSE close
#define TEST_FILENO fileno
#endif

#include "objects/probable_objects.h"
#include "objects/probable.h"
#include "util/random.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks weighted-pool admission, draws, overflow rejection, copying, and bounds.
int main() {
    ProbableObjects *po = ProbableObjects_1(4);
    CHECK(po != nullptr);
    CHECK(ProbableObjects_size(po) == 0);
    CHECK(ProbableObjects_capacity(po) >= 4);
    CHECK(ProbableObjects_totalWeight(po) == 0);

    CHECK(ProbableObjects_add(po, 100, 1) == 1);
    CHECK(ProbableObjects_add(po, 200, 2) == 1);
    CHECK(ProbableObjects_size(po) == 2);
    CHECK(ProbableObjects_totalWeight(po) == 3);

    // Cumulative column: 1, then 3.
    CHECK(ProbableObjects_objectAt(po, 0) == 100);
    CHECK(ProbableObjects_weightAt(po, 0) == 1);
    CHECK(ProbableObjects_cumulativeAt(po, 0) == 1);
    CHECK(ProbableObjects_objectAt(po, 1) == 200);
    CHECK(ProbableObjects_cumulativeAt(po, 1) == 3);

    // A draw lands on one of the pool's objects.
    pointer drawn = ProbableObjects_get(po);
    CHECK(drawn == 100 || drawn == 200);

    // A zero-weight first slot must never capture zero-based target 0.
    ProbableObjects *weighted = ProbableObjects_1(2);
    CHECK(weighted != nullptr);
    CHECK(ProbableObjects_add(weighted, 11, 0) == 1);
    CHECK(ProbableObjects_add(weighted, 22, 1) == 1);
    Random *stream = Random_2(123, RANDOM_ENGINE_MURMUR);
    CHECK(stream != nullptr);
    if (stream && weighted) {
        for (bytes i = 0; i < 1000; i++)
            CHECK(Random_probablePool(stream, weighted) == 22);
    }
    Random_free(stream);
    ProbableObjects_free(weighted);

    // capacity overflow is refused, not corrupting.
    ProbableObjects *small = ProbableObjects_1(2);
    CHECK(ProbableObjects_add(small, 1, 1) == 1);
    CHECK(ProbableObjects_add(small, 2, 1) == 1);
    CHECK(ProbableObjects_add(small, 3, 1) == 0); // full
    CHECK(ProbableObjects_size(small) == 2);
    ProbableObjects_free(small);

    // The allocator and pool use u32 lengths. Neither constructor
    // arithmetic nor cumulative weights may silently wrap.
    ProbableObjects *boundary = ProbableObjects_1(2);
    CHECK(boundary != nullptr);
    CHECK(ProbableObjects_add(boundary, 77, UINT32_MAX) == 1);

    // Capture the cold diagnostic channel as well as the rejected results.
    FILE *capture = tmpfile();
    CHECK(capture != nullptr);
    if (capture) {
        fflush(stderr);
        int saved = TEST_DUP(TEST_FILENO(stderr));
        CHECK(saved >= 0);
        if (saved >= 0) {
            CHECK(TEST_DUP2(TEST_FILENO(capture), TEST_FILENO(stderr)) >= 0);
            CHECK(ProbableObjects_1(SIZE_MAX) == nullptr);
            CHECK(ProbableObjects_add(boundary, 88, 1) == 0);
            fflush(stderr);
            CHECK(TEST_DUP2(saved, TEST_FILENO(stderr)) >= 0);
            TEST_CLOSE(saved);
            rewind(capture);
            char log[512] = { 0 };
            bytes readCount = fread(log, 1, sizeof(log) - 1, capture);
            log[readCount] = '\0';
            CHECK(strstr(log, "[vex]") != nullptr);
            CHECK(strstr(log, "capacity is not representable") != nullptr);
            CHECK(strstr(log, "total weight overflow") != nullptr);
        }
        fclose(capture);
    }
    CHECK(ProbableObjects_size(boundary) == 1);
    CHECK(ProbableObjects_totalWeight(boundary) == UINT32_MAX);
    CHECK(ProbableObjects_objectAt(boundary, 0) == 77);
    CHECK(ProbableObjects_weightAt(boundary, 0) == UINT32_MAX);
    CHECK(ProbableObjects_objectAt(boundary, 1) == 0);
    ProbableObjects_free(boundary);

    // addProbable copies object + weight.
    Probable *pr = Probable_3((pointer) 500, 5, 5);
    CHECK(ProbableObjects_addProbable(po, pr) == 1);
    CHECK(ProbableObjects_size(po) == 3);
    CHECK(ProbableObjects_totalWeight(po) == 8);
    CHECK(ProbableObjects_objectAt(po, 2) == 500);
    Probable_free(pr);

    // The single-element array constructor must preserve flexible slots.
    ProbableObjects *copy = ProbableObjects_2(po, 1);
    CHECK(copy != nullptr);
    if (copy) {
        CHECK(ProbableObjects_capacity(copy) == 4);
        CHECK(ProbableObjects_size(copy) == 3);
        CHECK(ProbableObjects_objectAt(copy, 2) == 500);
        CHECK(ProbableObjects_cumulativeAt(copy, 2) == 8);
        CHECK(ProbableObjects_add(copy, 600, 1) == 1);
        CHECK(ProbableObjects_size(po) == 3);
        ProbableObjects_free(copy);
    }
    // Multiple populated flexible-array elements cannot be safely indexed.
    CHECK(ProbableObjects_2(po, 2) == nullptr);
    CHECK(ProbableObjects_2(nullptr, SIZE_MAX) == nullptr);

    // Null-safety.
    CHECK(ProbableObjects_size(nullptr) == 0);
    CHECK(ProbableObjects_capacity(nullptr) == 0);
    CHECK(ProbableObjects_totalWeight(nullptr) == 0);
    CHECK(ProbableObjects_add(nullptr, 1, 1) == 0);
    CHECK(ProbableObjects_objectAt(nullptr, 0) == 0);
    CHECK(ProbableObjects_cumulativeAt(nullptr, 0) == 0);
    CHECK(ProbableObjects_get(nullptr) == 0);
    ProbableObjects_free(nullptr);

    ProbableObjects_free(po);

    if (g_failures == 0) {
        printf("probable_objects_test: all assertions held\n");
        return 0;
    }
    printf("probable_objects_test: %d FAILURES\n", g_failures);
    return 1;
}
