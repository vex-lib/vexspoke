// vexspoke/test/util/random_test.c — the Random class _test.
//
// Proves util/random.c: the engine is chosen deterministically from the seed;
// each engine is reproducible from its own seed; forced engines are reported
// back and diverge; xorshift's zero-seed guard holds; draw ranges; weighted
// sampling.
//
// STATED GAP: Random_probablePool needs a ProbableObjects pool and is owned by
// the objects/ owner test.

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "util/random.h"
#include "objects/probable.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Compares two independently created generators for identical seeded output.
// Confirms repeated engines initialized from one seed produce the same stream.
// Compares two independently constructed engines for identical seeded output.
static bool sameStream(u64 seed, RandomEngine engine) {
    Random *a = Random_2(seed, engine);
    Random *b = Random_2(seed, engine);
    bool ok = (a != nullptr) && (b != nullptr);
    for (int i = 0; i < 32 && ok; i++)
        if (Random_nextLong(a) != Random_nextLong(b))
            ok = false;
    if (a) Random_free(a);
    if (b) Random_free(b);
    return ok;
}

// Tests seeded engine selection, reproducibility, ranges, weighted sampling, and null safety.
// Verifies seeded random engines are reproducible and exercise their range contracts.
// Verifies seed selection, reproducible streams, bounded draws, and weighted sampling.
int main(void) {
    // The zero-argument chooser creates an independently owned casual stream.
    Random *defaultStream = Random();
    CHECK(defaultStream != nullptr);
    (void) Random_nextLong(defaultStream);
    Random_free(defaultStream);

    // Auto-selected engine: same seed -> same engine and same stream.
    Random *p = Random(1234u);
    Random *q = Random(1234u);
    CHECK(p && q);
    CHECK(Random_engine(p) == Random_engine(q));
    for (int i = 0; i < 32; i++)
        CHECK(Random_nextLong(p) == Random_nextLong(q));
    Random_free(p);
    Random_free(q);

    // Engine selection is a pure function of the seed.
    Random *sameEngineA = Random(42u);
    Random *sameEngineB = Random(42u);
    CHECK(Random_engine(sameEngineA) == Random_engine(sameEngineB));
    Random_free(sameEngineA);
    Random_free(sameEngineB);

    // Each engine is reproducible from its own seed.
    CHECK(sameStream(1234u, RANDOM_ENGINE_MURMUR));
    CHECK(sameStream(1234u, RANDOM_ENGINE_XORSHIFT));
    CHECK(sameStream(1234u, RANDOM_ENGINE_PCG));

    // A forced engine is reported back, and distinct engines diverge.
    Random *m = Random_2(7u, RANDOM_ENGINE_MURMUR);
    Random *x = Random_2(7u, RANDOM_ENGINE_XORSHIFT);
    Random *c = Random_2(7u, RANDOM_ENGINE_PCG);
    CHECK(Random_engine(m) == RANDOM_ENGINE_MURMUR);
    CHECK(Random_engine(x) == RANDOM_ENGINE_XORSHIFT);
    CHECK(Random_engine(c) == RANDOM_ENGINE_PCG);
    CHECK(Random_nextLong(m) != Random_nextLong(x));
    CHECK(Random_nextLong(x) != Random_nextLong(c));
    Random_free(m);
    Random_free(x);
    Random_free(c);

    // xorshift's zero-seed guard: the stream is not stuck at 0.
    Random *z = Random_2(0u, RANDOM_ENGINE_XORSHIFT);
    u64 z0 = Random_nextLong(z);
    u64 z1 = Random_nextLong(z);
    CHECK(z0 != z1);
    Random_free(z);

    // Draw ranges hold on every engine.
    RandomEngine engines[] = { RANDOM_ENGINE_MURMUR, RANDOM_ENGINE_XORSHIFT, RANDOM_ENGINE_PCG };
    for (bytes e = 0; e < sizeof engines / sizeof engines[0]; e++) {
        Random *r = Random_2(42u, engines[e]);
        for (int i = 0; i < 32; i++) {
            f32 f = Random_nextFloat(r);
            CHECK(f >= 0.0f && f < 1.0f);
            d64 d = Random_nextf64(r);
            CHECK(d >= 0.0 && d < 1.0);
            (void) Random_nextInt(r);
            (void) Random_nextChar(r);
            CHECK(isfinite((d64) Random_nextNDCFloat(r)));
        }
        Random_free(r);
    }

    // Weighted sampling wrappers (engine-independent).
    Random *w = Random(99u);
    CHECK(Random_getWeight(w, 10u, 10u));
    CHECK(!Random_getWeight(w, 0u, 10u));

    pointer token = (pointer) &g_failures;
    Probable *hit = Probable_3(token, 10u, 10u);
    CHECK(Random_sample(w, hit) == token);
    Probable *miss = Probable_3(token, 0u, 10u);
    CHECK(Random_sample(w, miss) == 0u);
    Probable_free(hit);
    Probable_free(miss);

    // Null-safety.
    CHECK(Random_nextLong(nullptr) == 0);
    CHECK(Random_engine(nullptr) == RANDOM_ENGINE_MURMUR);
    CHECK(Random_sample(nullptr, hit) == 0u);
    CHECK(!Random_getWeight(nullptr, 1u, 1u));

    // The shared system stream is live and draws.
    Random *sys = Random_system();
    CHECK(sys != nullptr);
    (void) Random_nextLong(sys);

    Random_free(w);

    if (g_failures == 0) {
        printf("random_test: all assertions held\n");
        return 0;
    }
    printf("random_test: %d FAILURES\n", g_failures);
    return 1;
}
