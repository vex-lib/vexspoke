// vexspoke/test/algo/kd_tree_test.c — owner test for algo/kd_tree.
//
// Proves the balanced 3D k-d tree:
//   - build refuses nullptr / zero count; free is null-safe;
//   - nearest returns the exact closest point (with its payload and squared
//     distance) on a hand-computed cloud and against a brute-force oracle over
//     a deterministic pseudo-random cloud;
//   - nearest returns false for a null tree / null out pointer;
//   - queryRadius matches a brute-force radius count, obeys maxCount as a hard
//     cap, returns 0 for a negative radius / null out / zero cap, and includes
//     exact-boundary points;
//   - duplicate points are all retained;
//   - a single-point tree works.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "algo/kd_tree.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

static KdPoint mk(f32 x, f32 y, f32 z, u64 payload) {
    return KdPoint(x, y, z, payload);
}

static f32 dist_sq(const f32 a[3], const f32 b[3]) {
    f32 dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

static fn test_build_and_null() {
    KdPoint p = mk(1, 2, 3, 1);
    CHECK(KdTree_build(nullptr, 3) == nullptr);
    CHECK(KdTree_build(&p, 0) == nullptr);
    KdTree_free(nullptr);

    KdTree *t = KdTree_build(&p, 1);
    CHECK(t != nullptr);
    KdPoint out = KdPoint();
    f32 d = -1.0f;
    f32 q[3] = { 1, 2, 3 };
    CHECK(KdTree_nearest(t, q, &out, &d));
    CHECK(out.payload == 1);
    CHECK(d == 0.0f);
    // Null out pointer.
    CHECK(!KdTree_nearest(t, q, nullptr, &d));
    // A tree with a zero count would be null; null tree refused.
    CHECK(!KdTree_nearest(nullptr, q, &out, &d));
    KdTree_free(t);
}

static fn test_nearest_known() {
    KdPoint pts[] = {
        mk(0, 0, 0, 100),
        mk(10, 0, 0, 200),
        mk(0, 10, 0, 300),
        mk(0, 0, 10, 400),
        mk(5, 5, 5, 500),
    };
    bytes n = sizeof(pts) / sizeof(pts[0]);
    KdTree *t = KdTree_build(pts, n);
    CHECK(t != nullptr);

    KdPoint out;
    f32 d;
    f32 q1[3] = { 1, 0, 0 };
    CHECK(KdTree_nearest(t, q1, &out, &d));
    CHECK(out.payload == 100);                 // (0,0,0) is closest
    CHECK(fabsf(d - 1.0f) < 1e-6f);

    f32 q2[3] = { 4, 4, 4 };
    CHECK(KdTree_nearest(t, q2, &out, &d));
    CHECK(out.payload == 500);                 // (5,5,5)
    CHECK(fabsf(d - 3.0f) < 1e-5f);

    f32 q3[3] = { 9, 0.5f, 0 };
    CHECK(KdTree_nearest(t, q3, &out, &d));
    CHECK(out.payload == 200);                 // (10,0,0)

    KdTree_free(t);
}

static fn test_oracle_random() {
    enum { N = 400 };
    KdPoint pts[N], ref[N];
    u64 seed = 0x1234abcdull;
    for (bytes i = 0; i < N; i++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        f32 x = (f32) ((i32) (seed >> 33) % 200) * 0.5f - 50.0f;
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        f32 y = (f32) ((i32) (seed >> 33) % 200) * 0.5f - 50.0f;
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        f32 z = (f32) ((i32) (seed >> 33) % 200) * 0.5f - 50.0f;
        pts[i] = mk(x, y, z, (u64) i + 1);
        ref[i] = pts[i];
    }
    KdTree *t = KdTree_build(pts, N);
    CHECK(t != nullptr);

    f32 queries[][3] = { { 0, 0, 0 }, { 12.3f, -7.7f, 3.1f }, { -40.0f, 40.0f, -40.0f } };
    for (bytes qi = 0; qi < sizeof(queries) / sizeof(queries[0]); qi++) {
        // Brute force.
        f32 bestD = 1e30f;
        u64 bestPayload = 0;
        for (bytes i = 0; i < N; i++) {
            f32 d = dist_sq(queries[qi], ref[i].coord);
            if (d < bestD) {
                bestD = d;
                bestPayload = ref[i].payload;
            }
        }
        KdPoint out;
        f32 d = -1;
        CHECK(KdTree_nearest(t, queries[qi], &out, &d));
        CHECK(out.payload == bestPayload);
        CHECK(fabsf(d - bestD) < 1e-3f);
    }

    // Radius query matches a brute-force count.
    f32 center[3] = { 0, 0, 0 };
    f32 radius = 15.0f;
    bytes expected = 0;
    for (bytes i = 0; i < N; i++)
        if (dist_sq(center, ref[i].coord) <= radius * radius)
            expected++;

    u64 outPayloads[N];
    bytes got = KdTree_queryRadius(t, center, radius, outPayloads, N);
    CHECK(got == expected);

    // maxCount is a hard cap.
    bytes capped = KdTree_queryRadius(t, center, radius, outPayloads, 3);
    CHECK(capped == (expected < 3 ? expected : 3));

    // Radius 0 hits only exact coincidences (none here besides maybe round).
    CHECK(KdTree_queryRadius(t, center, 0.0f, outPayloads, N) == 0);
    // Negative radius refused.
    CHECK(KdTree_queryRadius(t, center, -1.0f, outPayloads, N) == 0);
    // Null / zero-cap refusals.
    CHECK(KdTree_queryRadius(t, center, radius, nullptr, N) == 0);
    CHECK(KdTree_queryRadius(t, center, radius, outPayloads, 0) == 0);
    CHECK(KdTree_queryRadius(nullptr, center, radius, outPayloads, N) == 0);

    KdTree_free(t);
}

static fn test_radius_boundary_and_duplicates() {
    KdPoint pts[] = {
        mk(0, 0, 0, 1),
        mk(3, 0, 0, 2),
        mk(0, 4, 0, 3),
        mk(1, 1, 1, 4),
        mk(1, 1, 1, 5),        // duplicate coordinate, distinct payload
    };
    bytes n = sizeof(pts) / sizeof(pts[0]);
    KdTree *t = KdTree_build(pts, n);
    CHECK(t != nullptr);

    // Radius 1 around origin includes (0,0,0) and both (1,1,1) duplicates
    // (distance squared 3 <= 1? no) — use radius sqrt(3) exactly.
    f32 q[3] = { 0, 0, 0 };
    u64 out[8];
    f32 r = 1.7320508f;                       // sqrt(3)
    bytes got = KdTree_queryRadius(t, q, r, out, 8);
    // (0,0,0) d=0; (1,1,1) x2 d=3. Total 3.
    CHECK(got == 3);
    bool saw1 = false, saw4 = false, saw5 = false;
    for (bytes i = 0; i < got; i++) {
        if (out[i] == 1) saw1 = true;
        if (out[i] == 4) saw4 = true;
        if (out[i] == 5) saw5 = true;
    }
    CHECK(saw1 && saw4 && saw5);

    KdTree_free(t);
}

int main() {
    CHECK(Memory_init(0));

    test_build_and_null();
    test_nearest_known();
    test_oracle_random();
    test_radius_boundary_and_duplicates();

    if (g_failures == 0) {
        printf("kd_tree_test: all assertions held\n");
        return 0;
    }
    printf("kd_tree_test: %d FAILURES\n", g_failures);
    return 1;
}
