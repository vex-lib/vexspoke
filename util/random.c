#include "vex.h"
#include "util/random.h"

#include <stdlib.h>
#include <time.h>

#if defined(VEX_PLATFORM_APPLE)
#include <mach/mach_time.h>
#endif

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/hash.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Random
 * ============================================================================
 * Deterministic pseudo-random stream with three interchangeable engines
 * (murmur/splitmix, xorshift64*, PCG64). The engine is chosen as a pure
 * function of the seed, so a seed fully determines the sequence and the caller
 * never selects or observes the algorithm. Random_2 forces an engine for tests
 * and reproduction. Random_system lazily seeds a process singleton from the
 * monotonic clock. Weighted sampling draws a uniform 63-bit value and compares
 * against weight/total, or binary-searches a ProbableObjects cumulative column;
 * totalWeight 0 falls back to uniform index selection.
 *
 * For secrets, tokens, or keys use security/SecureRandom — never this class:
 * this one is seedable and therefore predictable by design.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Random (util/random.c)
 * ============================================================================
 * the Random class, ported from util/Random.java.
 *
 * STRUCT FIELDS (Mirroring util/random.h):
 * ----------------------------------------------------------------------------
 *   Random {
 *     u64 seed; // primary state (engine-dependent)
 *     u64 inc; // secondary state (PCG low word; 0 otherwise)
 *     u32 counter; // sequence index (murmur engine)
 *     RandomEngine engine; // which algorithm this stream runs
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Random_1(seed)
 *   - Random_2(seed, engine)
 *
 * Core Functions:
 *   - Random_free(r)
 *   - Random_system(void)
 *   - Random_nextLong(r)
 *   - Random_nextInt(r)
 *   - Random_nextFloat(r)
 *   - Random_nextf64(r)
 *   - Random_nextNDCFloat(r)
 *   - Random_nextChar(r)
 *   - Random_sample(r, probable)
 *   - Random_probablePool(r, pool)
 *
 * Getters:
 *   - Random_engine(r)
 *   - Random_getWeight(r, weight, total)
 * ============================================================================
 */


// random.c — Random port (Legacy: util/Random.java).

static const u64 GOLDEN_RATIO_64 = 0x9e3779b97f4a7c15ull;

// PCG64 (XSH-RR): a 128-bit LCG multiplier and an odd increment.
static const __uint128_t PCG64_MULT =
    (((__uint128_t) 0x2360ED051FC65DA4ULL) << 64) | (__uint128_t) 0x4385DF649FCCF645ULL;
#define PCG64_INC ((__uint128_t) 1442695040888963407ULL)

static Random *system_rng = nullptr;

static u64 systemSeed(void) {
#if defined(VEX_PLATFORM_APPLE)
    u64 ticks = (u64) mach_absolute_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 ticks = (u64) ts.tv_sec * 1000000000ULL + (u64) ts.tv_nsec;
#endif
    // Mix (never just shift): a shift would throw the good high bits away.
    // Fold in the stream address so two streams made in the same tick diverge.
    return Hash_murmur3Mix64(ticks) ^ Hash_murmur3Mix64((u64)(pointer) &system_rng);
}

// --- Engines: one step yields one uniform uint64 ---

static u64 murmurStep(Random *r) {
    u64 mixed = (*r).seed ^ ((u64)(*r).counter * GOLDEN_RATIO_64);
    u64 result = Hash_murmur3Mix64(mixed);
    (*r).seed = result;
    (*r).counter++;
    return result;
}

static u64 xorshift64StarStep(Random *r) {
    u64 x = (*r).seed;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    (*r).seed = x;
    return x * 0x2545F4914F6CDD1DULL;
}

static u64 pcg64Step(Random *r) {
    __uint128_t state = ((__uint128_t)(*r).seed << 64) | (__uint128_t)(*r).inc;
    __uint128_t old = state;
    state = old * PCG64_MULT + PCG64_INC;
    u64 xored = (u64)((old >> 64) ^ old);
    unsigned rot = (unsigned)(old >> 122);
    (*r).seed = (u64)(state >> 64);
    (*r).inc = (u64) state;
    return (xored >> rot) | (xored << ((64u - rot) & 63u));
}

// The engine choice is a PURE function of the seed: identical seeds must give
// identical streams (the Deterministic Calculation Law forbids dependence on
// time, address, or allocation order).
static RandomEngine pickEngine(u64 seed) {
    return (RandomEngine)(Hash_murmur3Mix64(seed) % (u64) RANDOM_ENGINE_COUNT);
}

static void initState(Random *r, u64 seed, RandomEngine engine) {
    (*r).engine = engine;
    (*r).counter = 0;
    (*r).inc = 0;
    switch (engine) {
        case RANDOM_ENGINE_XORSHIFT:
            // 0 is xorshift's fixed point; remap it to a nonzero constant.
            (*r).seed = (seed != 0) ? seed : 0x9E3779B97F4A7C15ULL;
            break;
        case RANDOM_ENGINE_PCG:
            (*r).seed = seed;
            (*r).inc = seed;
            break;
        case RANDOM_ENGINE_MURMUR:
        default:
            (*r).seed = seed;
            break;
    }
}

Random *Random_2(u64 seed, RandomEngine engine) {
    if ((int) engine < 0 || engine >= RANDOM_ENGINE_COUNT)
        engine = RANDOM_ENGINE_MURMUR;
    Random *r = (Random*) Memory_alloc(TYPE_RANDOM, sizeof(Random));
    if (!r)
        return nullptr;
    initState(r, seed, engine);
    return r;
}

Random *Random_1(u64 seed) {
    return Random_2(seed, pickEngine(seed));
}

Random *Random_0(void) {
    return Random_1(systemSeed());
}

void Random_free(Random *r) {
    Memory_free(r);
}

RandomEngine Random_engine(const Random *r) {
    return r ? (*r).engine : RANDOM_ENGINE_MURMUR;
}

Random *Random_system(void) {
    if (!system_rng)
        system_rng = Random_1(systemSeed());
    return system_rng;
}

u64 Random_nextLong(Random *r) {
    if (!r) return 0;
    switch ((*r).engine) {
        case RANDOM_ENGINE_XORSHIFT: return xorshift64StarStep(r);
        case RANDOM_ENGINE_PCG:      return pcg64Step(r);
        case RANDOM_ENGINE_MURMUR:
        default:                     return murmurStep(r);
    }
}

i32 Random_nextInt(Random *r) {
    return (i32)Random_nextLong(r);
}

f32 Random_nextFloat(Random *r) {
    return (f32)((Random_nextLong(r) & 0xFFFFFF) / 16777216.0);
}

d64 Random_nextf64(Random *r) {
    return (d64)((Random_nextLong(r) & 0x1FFFFFFFFFFFFFull) / 9007199254740992.0);
}

f32 Random_nextNDCFloat(Random *r) {
    return (f32)(((Random_nextLong(r) & 0xFFFFFF) / 8388608.0) - 1.0);
}

char Random_nextChar(Random *r) {
    int64_t v = (int64_t)Random_nextLong(r);
    return (char)(32 + (llabs(v) % 95));
}

bool Random_getWeight(Random *r, u32 weight, u32 total) {
    if (!r) return false;
    if (total == 0) return false;
    if (weight >= total) return true;
    if (weight == 0) return false;
    u64 val = Random_nextLong(r) & 0x7FFFFFFFFFFFFFFFull;
    return (val % total) < weight;
}

pointer Random_sample(Random *r, const Probable *probable) {
    if (!probable) return 0;
    if (Random_getWeight(r, (*probable).weight, (*probable).total))
        return (*probable).object;
    return 0;
}

pointer Random_probablePool(Random *r, const ProbableObjects *pool) {
    if (!pool) return 0;
    bytes count = ProbableObjects_size((ProbableObjects*) pool);
    if (count == 0) return 0;

    u32 total_weight = ProbableObjects_totalWeight((ProbableObjects*) pool);
    u64 val = Random_nextLong(r) & 0x7FFFFFFFFFFFFFFFull;

    if (total_weight == 0) {
        bytes idx = (bytes)(val % count);
        return ProbableObjects_objectAt((ProbableObjects*) pool, idx);
    }

    u32 target = (u32)(val % total_weight);
    bytes low = 0;
    bytes high = count - 1;
    while (low < high) {
        bytes mid = (low + high) / 2;
        u32 cumulative = ProbableObjects_cumulativeAt((ProbableObjects*) pool, mid);
        // target is zero-based: a slot owns [previous cumulative, cumulative).
        // A zero-weight slot owns no target, including target zero.
        if (cumulative <= target)
            low = mid + 1;
        else
            high = mid;
    }
    return ProbableObjects_objectAt((ProbableObjects*) pool, low);
}
