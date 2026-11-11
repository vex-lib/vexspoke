#include "algo/dijkstra.h"

#include <stdlib.h>
#include <string.h>
#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Dijkstra
 * ============================================================================
 * Single-source shortest-path solver over weighted adjacency lists, used for
 * routing, spatial navigation, and audio node dependency graph. The graph is
 * a pair of contiguous off-heap arenas (nodes plus per-node edge arrays) so
 * relaxation walks cache-friendly memory; edge arrays d64 on demand and
 * are reclaimed with the graph. The priority queue is a private min-heap over
 * a caller-budgeted scratch arena sized 4N+16, so the hot search path performs
 * zero steady-state allocation. Unreachable goals return 0 with all scratch
 * freed; the path is reconstructed backward through the predecessor table and
 * reversed into the caller's dest-last outPath buffer.
 * ============================================================================
 */

;;OVERVIEW;
/**
 * ============================================================================
 * CLASS: Dijkstra (algo/dijkstra.c)
 * ============================================================================
 * Solves single-source shortest paths on adjacency lists using min-priority
 * relaxation. Operates over contiguous off-heap memory.
 *
 * STRUCT FIELDS (Mirroring algo/dijkstra.h):
 * ----------------------------------------------------------------------------
 *   DijkstraEdge {
 *     u32 target;   // destination node index
 *     f32 weight;      // edge cost
 *   }
 *   DijkstraNode {
 *     u32 edgeCount;     // live outgoing edges
 *     u32 edgeCapacity;  // allocated edge slots (doubling)
 *     DijkstraEdge *edges;    // contiguous edge arena
 *   }
 *   DijkstraGraph {
 *     u32 nodeCount;  // fixed node count at creation
 *     DijkstraNode *nodes; // contiguous node arena
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Dijkstra(nodeCount) / Dijkstra_1(nodeCount)
 *
 * Public Core Functions: (.h)
 *   - Dijkstra_free(graph)
 *   - Dijkstra_addEdge(graph, from, to, weight)
 *   - Dijkstra_addBiEdge(graph, a, b.sh, weight)
 *   - Dijkstra_shortestPath(graph, startNode, goalNode, outPath, maxPathNodes, outTotalDistance)
 *
 * Private Core Functions: (.c static)
 *   - MinQ_push(q, node, dist)
 *   - MinQ_pop(q, outNode, outDist)
 * ============================================================================
 */

DijkstraGraph *Dijkstra_1(u32 nodeCount) {
    if (nodeCount == 0) return nullptr;

    DijkstraGraph *graph = (DijkstraGraph*) Memory_alloc(TYPE_BYTE_ARRAY, sizeof(DijkstraGraph));
    if (graph == nullptr) return nullptr;

    (*graph).nodeCount = nodeCount;
    bytes nodesBytes = (bytes) nodeCount * sizeof(DijkstraNode);
    (*graph).nodes = (DijkstraNode*) Memory_alloc(TYPE_BYTE_ARRAY, nodesBytes);
    if ((*graph).nodes == nullptr) {
        Memory_free(graph);
        return nullptr;
    }

    for (u32 i = 0; i < nodeCount; i++) {
        (*graph).nodes[i].edgeCount = 0;
        (*graph).nodes[i].edgeCapacity = 0;
        (*graph).nodes[i].edges = nullptr;
    }

    return graph;
}

void Dijkstra_free(DijkstraGraph *graph) {
    if (graph == nullptr) return;
    if ((*graph).nodes != nullptr) {
        for (u32 i = 0; i < (*graph).nodeCount; i++) {
            if ((*graph).nodes[i].edges != nullptr) {
                Memory_free((*graph).nodes[i].edges);
                (*graph).nodes[i].edges = nullptr;
            }
        }
        Memory_free((*graph).nodes);
        (*graph).nodes = nullptr;
    }
    Memory_free(graph);
}

bool Dijkstra_addEdge(DijkstraGraph *graph, u32 from, u32 to, f32 weight) {
    if (graph == nullptr || from >= (*graph).nodeCount || to >= (*graph).nodeCount) {
        return false;
    }
    DijkstraNode *node = &(*graph).nodes[from];
    if ((*node).edgeCount >= (*node).edgeCapacity) {
        u32 newCap = ((*node).edgeCapacity == 0) ? 4 : (*node).edgeCapacity * 2;
        DijkstraEdge *newEdges = (DijkstraEdge*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) newCap * sizeof(DijkstraEdge));
        if (newEdges == nullptr) return false;
        if ((*node).edges != nullptr) {
            memcpy(newEdges, (*node).edges, (bytes) (*node).edgeCount * sizeof(DijkstraEdge));
            Memory_free((*node).edges);
        }
        (*node).edges = newEdges;
        (*node).edgeCapacity = newCap;
    }

    (*node).edges[(*node).edgeCount].target = to;
    (*node).edges[(*node).edgeCount].weight = weight;
    (*node).edgeCount++;
    return true;
}

bool Dijkstra_addBiEdge(DijkstraGraph *graph, u32 a, u32 b, f32 weight) {
    bool ok1 = Dijkstra_addEdge(graph, a, b, weight);
    bool ok2 = Dijkstra_addEdge(graph, b, a, weight);
    return ok1 && ok2;
}

// Min-heap entry for Dijkstra priority queue
typedef struct QItem {
    u32 node;
    f32    dist;
} QItem;

typedef struct MinQ {
    QItem   *items;
    u32 size;
    u32 capacity;
} MinQ;

static void MinQ_push(MinQ *q, u32 node, f32 dist) {
    if ((*q).size >= (*q).capacity) return;
    u32 i = (*q).size++;
    (*q).items[i].node = node;
    (*q).items[i].dist = dist;

    // Sift up
    while (i > 0) {
        u32 parent = (i - 1) / 2;
        if ((*q).items[i].dist < (*q).items[parent].dist) {
            QItem swap = (*q).items[i];
            (*q).items[i] = (*q).items[parent];
            (*q).items[parent] = swap;
            i = parent;
        } else {
            break;
        }
    }
}

static bool MinQ_pop(MinQ *q, u32 *outNode, f32 *outDist) {
    if ((*q).size == 0) return false;
    *outNode = (*q).items[0].node;
    *outDist = (*q).items[0].dist;

    (*q).size--;
    if ((*q).size > 0) {
        (*q).items[0] = (*q).items[(*q).size];
        // Sift down
        u32 i = 0;
        while (true) {
            u32 left = 2 * i + 1;
            u32 right = 2 * i + 2;
            u32 smallest = i;

            if (left < (*q).size && (*q).items[left].dist < (*q).items[smallest].dist) {
                smallest = left;
            }
            if (right < (*q).size && (*q).items[right].dist < (*q).items[smallest].dist) {
                smallest = right;
            }
            if (smallest != i) {
                QItem swap = (*q).items[i];
                (*q).items[i] = (*q).items[smallest];
                (*q).items[smallest] = swap;
                i = smallest;
            } else {
                break;
            }
        }
    }
    return true;
}

bytes Dijkstra_shortestPath(const DijkstraGraph *graph,
                             u32             startNode,
                             u32             goalNode,
                             u32            *outPath,
                             bytes               maxPathNodes,
                             f32               *outTotalDistance) {
    if (graph == nullptr || outPath == nullptr || maxPathNodes == 0) return 0;
    u32 N = (*graph).nodeCount;
    if (startNode >= N || goalNode >= N) return 0;

    f32 *dist = (f32*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) N * sizeof(f32));
    u32 *prev = (u32*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) N * sizeof(u32));
    bool *visited = (bool*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) N * sizeof(bool));
    QItem *qItems = (QItem*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) (N * 4 + 16) * sizeof(QItem));

    if (!dist || !prev || !visited || !qItems) {
        if (dist) Memory_free(dist);
        if (prev) Memory_free(prev);
        if (visited) Memory_free(visited);
        if (qItems) Memory_free(qItems);
        return 0;
    }

    for (u32 i = 0; i < N; i++) {
        dist[i] = DIJKSTRA_INFINITY;
        prev[i] = 0xFFFFFFFFu;
        visited[i] = false;
    }

    dist[startNode] = 0.0f;
    MinQ q;
    q.items = qItems;
    q.size = 0;
    q.capacity = N * 4 + 16;
    MinQ_push(&q, startNode, 0.0f);

    while (q.size > 0) {
        u32 u;
        f32 d;
        if (!MinQ_pop(&q, &u, &d)) break;

        if (visited[u]) continue;
        visited[u] = true;

        if (u == goalNode) break;

        const DijkstraNode *node = &(*graph).nodes[u];
        for (u32 e = 0; e < (*node).edgeCount; e++) {
            u32 v = (*node).edges[e].target;
            f32 w = (*node).edges[e].weight;
            if (!visited[v] && dist[u] + w < dist[v]) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                MinQ_push(&q, v, dist[v]);
            }
        }
    }

    if (dist[goalNode] >= DIJKSTRA_INFINITY) {
        // Unreachable
        Memory_free(dist);
        Memory_free(prev);
        Memory_free(visited);
        Memory_free(qItems);
        return 0;
    }

    if (outTotalDistance != nullptr) {
        *outTotalDistance = dist[goalNode];
    }

    // Reconstruct path backward
    u32 tempPath[1024];
    bytes pathLen = 0;
    u32 curr = goalNode;
    while (curr != 0xFFFFFFFFu && pathLen < 1024) {
        tempPath[pathLen++] = curr;
        if (curr == startNode) break;
        curr = prev[curr];
    }

    // Reverse into outPath
    bytes count = (pathLen < maxPathNodes) ? pathLen : maxPathNodes;
    for (bytes i = 0; i < count; i++) {
        outPath[i] = tempPath[pathLen - 1 - i];
    }

    Memory_free(dist);
    Memory_free(prev);
    Memory_free(visited);
    Memory_free(qItems);
    return count;
}
