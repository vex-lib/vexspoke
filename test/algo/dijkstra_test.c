// vexspoke/test/algo/dijkstra_test.c — owner test for algo/dijkstra.
//
// Proves the weighted single-source shortest-path solver:
//   - create(0) refused; addEdge rejects out-of-range endpoints; addBiEdge
//     adds both directions; directed edges are asymmetric;
//   - a known graph returns the exact path and total distance (a longer direct
//     edge loses to the two-hop route);
//   - start == goal is the trivial one-node, zero-distance path;
//   - unreachable goals return 0 and never touch the distance sink;
//   - maxPathNodes truncates the returned count; nullptr path / graph / zero
//     capacity are safe refusals; out-of-range endpoints return 0;
//   - edge arrays grow past their initial capacity (4) and remain correct;
//   - a 128-node chain yields the exact cumulative distance;
//   - nullptr safety on free.

#include "vex.h"
#include <stdio.h>
#include <math.h>

#include "algo/dijkstra.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

/** Checks graph creation, invalid endpoints, bidirectional-edge rejection, and null-safe cleanup. */
static fn test_construction() {
    CHECK(Dijkstra(0) == nullptr);
    DijkstraGraph *g = Dijkstra(4);
    CHECK(g != nullptr);

    CHECK(!Dijkstra_addEdge(g, 4, 0, 1.0f));        // from out of range
    CHECK(!Dijkstra_addEdge(g, 0, 4, 1.0f));        // to out of range
    CHECK(!Dijkstra_addEdge(nullptr, 0, 1, 1.0f));

    CHECK(!Dijkstra_addBiEdge(g, 0, 9, 1.0f));      // one side invalid
    Dijkstra_free(g);
    Dijkstra_free(nullptr);
}

/** Verifies shortest-path choice, directed traversal, trivial paths, and unchanged distance on absence. */
static fn test_known_path() {
    DijkstraGraph *g = Dijkstra(4);                 // 0,1,2,3
    CHECK(g != nullptr);
    CHECK(Dijkstra_addBiEdge(g, 0, 1, 1.0f));
    CHECK(Dijkstra_addBiEdge(g, 1, 2, 2.0f));
    CHECK(Dijkstra_addEdge(g, 0, 2, 10.0f));        // direct but longer

    u32 path[8];
    f32 dist = -1.0f;
    bytes n = Dijkstra_shortestPath(g, 0, 2, path, 8, &dist);
    CHECK(n == 3);
    CHECK(path[0] == 0 && path[1] == 1 && path[2] == 2);
    CHECK(fabsf(dist - 3.0f) < 1e-5f);

    // start == goal: single node, distance 0.
    dist = -1.0f;
    n = Dijkstra_shortestPath(g, 2, 2, path, 8, &dist);
    CHECK(n == 1);
    CHECK(path[0] == 2);
    CHECK(dist == 0.0f);

    // Unreachable node 3: return 0, distance sink untouched.
    dist = 123.0f;
    n = Dijkstra_shortestPath(g, 0, 3, path, 8, &dist);
    CHECK(n == 0);
    CHECK(dist == 123.0f);

    // Directional edge 0 -> 2 exists but not 2 -> 0 directly; 2 -> 0 still
    // reachable via 2 -> 1 -> 0 (weight 3), proving directed traversal.
    dist = -1.0f;
    n = Dijkstra_shortestPath(g, 2, 0, path, 8, &dist);
    CHECK(n == 3 && path[0] == 2 && path[1] == 1 && path[2] == 0);
    CHECK(fabsf(dist - 3.0f) < 1e-5f);

    Dijkstra_free(g);
}

/** Exercises bounded path output and the solver's documented null and range refusals. */
static fn test_truncation_and_nulls() {
    DijkstraGraph *g = Dijkstra(3);
    CHECK(g != nullptr);
    CHECK(Dijkstra_addEdge(g, 0, 1, 1.0f));
    CHECK(Dijkstra_addEdge(g, 1, 2, 1.0f));

    u32 path[8];
    f32 dist = -1.0f;
    // Truncated output buffer.
    bytes n = Dijkstra_shortestPath(g, 0, 2, path, 2, &dist);
    CHECK(n == 2);
    CHECK(fabsf(dist - 2.0f) < 1e-5f);

    // Safe refusals.
    CHECK(Dijkstra_shortestPath(nullptr, 0, 2, path, 8, &dist) == 0);
    CHECK(Dijkstra_shortestPath(g, 0, 2, nullptr, 8, &dist) == 0);
    CHECK(Dijkstra_shortestPath(g, 0, 2, path, 0, &dist) == 0);
    CHECK(Dijkstra_shortestPath(g, 5, 2, path, 8, &dist) == 0);   // start OOR
    CHECK(Dijkstra_shortestPath(g, 0, 5, path, 8, &dist) == 0);   // goal OOR

    // A nullptr distance sink is allowed.
    n = Dijkstra_shortestPath(g, 0, 2, path, 8, nullptr);
    CHECK(n == 3);

    Dijkstra_free(g);
}

/** Adds enough outgoing edges to grow adjacency storage and checks the resulting shortest path. */
static fn test_edge_growth() {
    DijkstraGraph *g = Dijkstra(32);
    CHECK(g != nullptr);
    // Node 0 gets 20 outgoing edges, forcing the initial capacity 4 to d64
    // several times.
    for (u32 i = 1; i <= 20; i++)
        CHECK(Dijkstra_addEdge(g, 0, i, (f32) i));

    // Direct edge to 20 has weight 20; the cheapest route is direct here, so
    // the path is [0, 20] and distance 20.
    u32 path[4];
    f32 dist = -1.0f;
    bytes n = Dijkstra_shortestPath(g, 0, 20, path, 4, &dist);
    CHECK(n == 2 && path[0] == 0 && path[1] == 20);
    CHECK(fabsf(dist - 20.0f) < 1e-5f);

    Dijkstra_free(g);
}

/** Verifies exact path and distance on a long directed chain, including reverse unreachability. */
static fn test_chain() {
    enum { N = 128 };
    DijkstraGraph *g = Dijkstra(N);
    CHECK(g != nullptr);
    for (u32 i = 0; i + 1 < N; i++)
        CHECK(Dijkstra_addEdge(g, i, i + 1, 0.5f));

    u32 path[N];
    f32 dist = -1.0f;
    bytes n = Dijkstra_shortestPath(g, 0, N - 1, path, N, &dist);
    CHECK(n == N);
    for (bytes i = 0; i < N; i++)
        CHECK(path[i] == i);
    CHECK(fabsf(dist - 0.5f * (f32) (N - 1)) < 1e-3f);

    // The reverse direction is unreachable in this one-way chain.
    CHECK(Dijkstra_shortestPath(g, N - 1, 0, path, N, &dist) == 0);

    Dijkstra_free(g);
}

/** Runs the Dijkstra owner scenarios and returns failure if any assertion was violated. */
int main() {
    CHECK(Memory_init(0));

    test_construction();
    test_known_path();
    test_truncation_and_nulls();
    test_edge_growth();
    test_chain();

    if (g_failures == 0) {
        printf("dijkstra_test: all assertions held\n");
        return 0;
    }
    printf("dijkstra_test: %d FAILURES\n", g_failures);
    return 1;
}
