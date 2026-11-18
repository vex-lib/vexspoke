#include "vex.h"
#include "algo/dijkstra.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;GETTER
static inline u32 DijkstraGraph_nodeCount(const DijkstraGraph *s) { return (*s).nodeCount; }
;;SETTER
static inline fn DijkstraGraph_setNodeCount(DijkstraGraph *s, u32 v) { (*s).nodeCount = v; }
;;GETTER
static inline DijkstraNode * DijkstraGraph_nodes(const DijkstraGraph *s) { return (*s).nodes; }
;;SETTER
static inline fn DijkstraGraph_setNodes(DijkstraGraph *s, DijkstraNode * v) { (*s).nodes = v; }

;;GETTER
static inline u32 DijkstraNode_edgeCapacity(const DijkstraNode *s) { return (*s).edgeCapacity; }
;;SETTER
static inline fn DijkstraNode_setEdgeCapacity(DijkstraNode *s, u32 v) { (*s).edgeCapacity = v; }
;;GETTER
static inline u32 DijkstraNode_edgeCount(const DijkstraNode *s) { return (*s).edgeCount; }
;;SETTER
static inline fn DijkstraNode_setEdgeCount(DijkstraNode *s, u32 v) { (*s).edgeCount = v; }
;;GETTER
static inline DijkstraEdge * DijkstraNode_edges(const DijkstraNode *s) { return (*s).edges; }
;;SETTER
static inline fn DijkstraNode_setEdges(DijkstraNode *s, DijkstraEdge * v) { (*s).edges = v; }


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

    DijkstraGraph_setNodeCount(graph, nodeCount);
    bytes nodesBytes = (bytes) nodeCount * sizeof(DijkstraNode);
    DijkstraGraph_setNodes(graph, (DijkstraNode*) Memory_alloc(TYPE_BYTE_ARRAY, nodesBytes));
    if (DijkstraGraph_nodes(graph) == nullptr) {
        Memory_free(graph);
        return nullptr;
    }

    for (u32 i = 0; i < nodeCount; i++) {
        DijkstraGraph_nodes(graph)[i].edgeCount = 0;
        DijkstraGraph_nodes(graph)[i].edgeCapacity = 0;
        DijkstraGraph_nodes(graph)[i].edges = nullptr;
    }

    return graph;
}

fn Dijkstra_free(DijkstraGraph *graph) {
    if (graph == nullptr) return;
    if (DijkstraGraph_nodes(graph) != nullptr) {
        for (u32 i = 0; i < DijkstraGraph_nodeCount(graph); i++) {
            if (DijkstraGraph_nodes(graph)[i].edges != nullptr) {
                Memory_free(DijkstraGraph_nodes(graph)[i].edges);
                DijkstraGraph_nodes(graph)[i].edges = nullptr;
            }
        }
        Memory_free(DijkstraGraph_nodes(graph));
        DijkstraGraph_setNodes(graph, nullptr);
    }
    Memory_free(graph);
}

bool Dijkstra_addEdge(DijkstraGraph *graph, u32 from, u32 to, f32 weight) {
    if (graph == nullptr || from >= DijkstraGraph_nodeCount(graph) || to >= DijkstraGraph_nodeCount(graph)) {
        return false;
    }
    DijkstraNode *node = &DijkstraGraph_nodes(graph)[from];
    if (DijkstraNode_edgeCount(node) >= DijkstraNode_edgeCapacity(node)) {
        u32 newCap = (DijkstraNode_edgeCapacity(node) == 0) ? 4 : DijkstraNode_edgeCapacity(node) * 2;
        DijkstraEdge *newEdges = (DijkstraEdge*) Memory_alloc(TYPE_BYTE_ARRAY, (bytes) newCap * sizeof(DijkstraEdge));
        if (newEdges == nullptr) return false;
        if (DijkstraNode_edges(node) != nullptr) {
            memcpy(newEdges, DijkstraNode_edges(node), (bytes) DijkstraNode_edgeCount(node) * sizeof(DijkstraEdge));
            Memory_free(DijkstraNode_edges(node));
        }
        DijkstraNode_setEdges(node, newEdges);
        DijkstraNode_setEdgeCapacity(node, newCap);
    }

    DijkstraNode_edges(node)[DijkstraNode_edgeCount(node)].target = to;
    DijkstraNode_edges(node)[DijkstraNode_edgeCount(node)].weight = weight;
    DijkstraNode_setEdgeCount(node, DijkstraNode_edgeCount(node) + 1u);
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

;;GETTER
static inline u32 MinQ_capacity(const MinQ *s) { return (*s).capacity; }
;;GETTER
static inline QItem * MinQ_items(const MinQ *s) { return (*s).items; }
;;GETTER
static inline u32 MinQ_size(const MinQ *s) { return (*s).size; }
;;GETTER
static inline u32 *MinQ_sizeMut(MinQ *s) { return &(*s).size; }
;;SETTER
static inline fn MinQ_setSize(MinQ *s, u32 v) { (*s).size = v; }


static fn MinQ_push(MinQ *q, u32 node, f32 dist) {
    if (MinQ_size(q) >= MinQ_capacity(q)) return;
    u32 i = (*MinQ_sizeMut(q))++;
    MinQ_items(q)[i].node = node;
    MinQ_items(q)[i].dist = dist;

    // Sift up
    while (i > 0) {
        u32 parent = (i - 1) / 2;
        if (MinQ_items(q)[i].dist < MinQ_items(q)[parent].dist) {
            QItem swap = MinQ_items(q)[i];
            MinQ_items(q)[i] = MinQ_items(q)[parent];
            MinQ_items(q)[parent] = swap;
            i = parent;
        } else {
            break;
        }
    }
}

static bool MinQ_pop(MinQ *q, u32 *outNode, f32 *outDist) {
    if (MinQ_size(q) == 0) return false;
    *outNode = MinQ_items(q)[0].node;
    *outDist = MinQ_items(q)[0].dist;

    MinQ_setSize(q, MinQ_size(q) - 1u);
    if (MinQ_size(q) > 0) {
        MinQ_items(q)[0] = MinQ_items(q)[MinQ_size(q)];
        // Sift down
        u32 i = 0;
        while (true) {
            u32 left = 2 * i + 1;
            u32 right = 2 * i + 2;
            u32 smallest = i;

            if (left < MinQ_size(q) && MinQ_items(q)[left].dist < MinQ_items(q)[smallest].dist) {
                smallest = left;
            }
            if (right < MinQ_size(q) && MinQ_items(q)[right].dist < MinQ_items(q)[smallest].dist) {
                smallest = right;
            }
            if (smallest != i) {
                QItem swap = MinQ_items(q)[i];
                MinQ_items(q)[i] = MinQ_items(q)[smallest];
                MinQ_items(q)[smallest] = swap;
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
    u32 N = DijkstraGraph_nodeCount(graph);
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

        const DijkstraNode *node = &DijkstraGraph_nodes(graph)[u];
        for (u32 e = 0; e < DijkstraNode_edgeCount(node); e++) {
            u32 v = DijkstraNode_edges(node)[e].target;
            f32 w = DijkstraNode_edges(node)[e].weight;
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
