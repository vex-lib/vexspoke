#ifndef ALGO_DIJKSTRA_H
#define ALGO_DIJKSTRA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lang/constructor.h"

// algo/dijkstra.h — Shortest-Path Graph Solver.
//
// Solves single-source shortest paths on weighted directed/undirected graph
// using a priority queue (MinHeap). Used for routing, spatial navigation,
// and audio node dependency graph.

#define DIJKSTRA_INFINITY 1e30f

typedef struct DijkstraEdge {
    u32 target;
    f32    weight;
} DijkstraEdge;

typedef struct DijkstraNode {
    u32      edgeCount;
    u32      edgeCapacity;
    DijkstraEdge *edges;
} DijkstraNode;

typedef struct DijkstraGraph {
    u32      nodeCount;
    DijkstraNode *nodes;
} DijkstraGraph;

DijkstraGraph *Dijkstra_1(u32 nodeCount);
#define Dijkstra(...) CONSTRUCTOR_DISPATCH(Dijkstra, __VA_ARGS__)
void           Dijkstra_free(DijkstraGraph *graph);

bool Dijkstra_addEdge(DijkstraGraph *graph, u32 from, u32 to, f32 weight);
bool Dijkstra_addBiEdge(DijkstraGraph *graph, u32 a, u32 b, f32 weight);

// Solve shortest path from start to goal.
// Dest-last order: writes node indices into outPath and path length into outTotalDist.
// Returns number of nodes in the path (0 if unreachable).
bytes Dijkstra_shortestPath(const DijkstraGraph *graph,
                             u32             startNode,
                             u32             goalNode,
                             u32            *outPath,
                             bytes               maxPathNodes,
                             f32               *outTotalDistance);

#endif
