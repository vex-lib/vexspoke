#include "vex.h"
#include "algo/tree_sit.h"

#include <stdlib.h>
#include <string.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: TreeSit
 * ============================================================================
 * Relational AST tree-sitter graph structure: a flat, dynamically-grown node
 * array where each node carries parent/sibling/first-child links plus a
 * byte/row-col span, enabling pre-order, post-order, and span-based queries
 * without pointer chasing. Exists because language grammars (R3) and editors
 * (R4/R5) need a stable, allocation-friendly syntax tree that hot-reloadable
 * modules can build and walk. memory: the node array f64s exponentially
 * (the Dynamic Scalability & Anti-Hardcoding Law); the root is a signed index
 * (-1 when empty). Lifetime: the TreeSitTree(...)/TreeSitTree_destroy pair;
 * symbol names are borrowed, never owned.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: TreeSit (algo/tree_sit.c)
 * ============================================================================
 * Relational AST tree-sitter graph structure with pre-order, post-order, and
 * span-based query capabilities.
 *
 * STRUCT FIELDS:
 *   - TreeSitSpan: byte and row/col coordinates
 *   - TreeSitNode: parent, sibling, first_child links, payload
 *   - TreeSitTree: dynamic node array and root pointer
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Tree Lifecycle:
 *   - TreeSitTree_0()
 *   - TreeSitTree_1(initial_capacity)
 *   - TreeSitTree_destroy(tree)
 *   - TreeSitTree_add_node(parent_idx, kind, symbol_name, span, tree)
 * Traversal & Search:
 *   - TreeSitTree_walk_preorder(tree, visitor, user_data)
 *   - TreeSitTree_walk_postorder(tree, visitor, user_data)
 *   - TreeSitTree_find_by_byte(tree, byte_offset)
 *   - TreeSitTree_count_kind(tree, kind)
 * ============================================================================
 */


;;GETTER
static inline u32 TreeSitNode_child_count(const TreeSitNode *s) { return (*s).child_count; }
;;SETTER
static inline fn TreeSitNode_setChild_count(TreeSitNode *s, u32 v) { (*s).child_count = v; }
;;GETTER
static inline i32 TreeSitNode_first_child(const TreeSitNode *s) { return (*s).first_child; }
;;SETTER
static inline fn TreeSitNode_setFirst_child(TreeSitNode *s, i32 v) { (*s).first_child = v; }
;;SETTER
static inline fn TreeSitNode_setId(TreeSitNode *s, u32 v) { (*s).id = v; }
;;SETTER
static inline fn TreeSitNode_setKind(TreeSitNode *s, u16 v) { (*s).kind = v; }
;;GETTER
static inline i32 TreeSitNode_next_sibling(const TreeSitNode *s) { return (*s).next_sibling; }
;;SETTER
static inline fn TreeSitNode_setNext_sibling(TreeSitNode *s, i32 v) { (*s).next_sibling = v; }
;;SETTER
static inline fn TreeSitNode_setParent(TreeSitNode *s, i32 v) { (*s).parent = v; }
;;SETTER
static inline fn TreeSitNode_setPayload(TreeSitNode *s, any v) { (*s).payload = v; }
;;SETTER
static inline fn TreeSitNode_setPrev_sibling(TreeSitNode *s, i32 v) { (*s).prev_sibling = v; }
;;GETTER
static inline TreeSitSpan TreeSitNode_span(const TreeSitNode *s) { return (*s).span; }
;;SETTER
static inline fn TreeSitNode_setSpan(TreeSitNode *s, TreeSitSpan v) { (*s).span = v; }
;;SETTER
static inline fn TreeSitNode_setSymbol_name(TreeSitNode *s, const char * v) { (*s).symbol_name = v; }

;;GETTER
static inline u32 TreeSitTree_node_capacity(const TreeSitTree *s) { return (*s).node_capacity; }
;;SETTER
static inline fn TreeSitTree_setNode_capacity(TreeSitTree *s, u32 v) { (*s).node_capacity = v; }
;;GETTER
static inline u32 TreeSitTree_node_count(const TreeSitTree *s) { return (*s).node_count; }
;;GETTER
static inline u32 *TreeSitTree_node_countMut(TreeSitTree *s) { return &(*s).node_count; }
;;SETTER
static inline fn TreeSitTree_setNode_count(TreeSitTree *s, u32 v) { (*s).node_count = v; }
;;GETTER
static inline TreeSitNode * TreeSitTree_nodes(const TreeSitTree *s) { return (*s).nodes; }
;;SETTER
static inline fn TreeSitTree_setNodes(TreeSitTree *s, TreeSitNode * v) { (*s).nodes = v; }
;;GETTER
static inline i32 TreeSitTree_root(const TreeSitTree *s) { return (*s).root; }
;;SETTER
static inline fn TreeSitTree_setRoot(TreeSitTree *s, i32 v) { (*s).root = v; }

;;INTENTION("AST node graph representation and hierarchy traversal")

TreeSitTree TreeSitTree_0() {
    return TreeSitTree_1(16u);
}

TreeSitTree TreeSitTree_1(u32 initial_capacity) {
    TreeSitTree tree;
    memset(&tree, 0, sizeof tree);
    u32 cap = (initial_capacity < 16) ? 16 : initial_capacity;
    tree.root = -1;
    tree.nodes = (TreeSitNode*) malloc(cap * sizeof(TreeSitNode));
    if (tree.nodes != nullptr)
        tree.node_capacity = cap;
    return tree;
}

fn TreeSitTree_destroy(TreeSitTree *tree) {
    if (!tree) {
        return;
    }
    if (TreeSitTree_nodes(tree)) {
        free(TreeSitTree_nodes(tree));
        TreeSitTree_setNodes(tree, nullptr);
    }
    TreeSitTree_setNode_count(tree, 0);
    TreeSitTree_setNode_capacity(tree, 0);
    TreeSitTree_setRoot(tree, -1);
}

i32 TreeSitTree_add_node(
    i32 parent_idx,
    u16 kind,
    const char *symbol_name,
    TreeSitSpan span,
    TreeSitTree *tree
) {
    if (!tree) {
        return -1;
    }

    if (TreeSitTree_node_count(tree) >= TreeSitTree_node_capacity(tree)) {
        u32 new_cap = TreeSitTree_node_capacity(tree) * 2;
        TreeSitNode *new_nodes = (TreeSitNode*) realloc(TreeSitTree_nodes(tree), new_cap * sizeof(TreeSitNode));
        if (!new_nodes) {
            return -1;
        }
        TreeSitTree_setNodes(tree, new_nodes);
        TreeSitTree_setNode_capacity(tree, new_cap);
    }

    i32 new_idx = (i32) (*TreeSitTree_node_countMut(tree))++;
    TreeSitNode *node = &TreeSitTree_nodes(tree)[new_idx];

    TreeSitNode_setId(node, (u32) new_idx);
    TreeSitNode_setKind(node, kind);
    TreeSitNode_setSymbol_name(node, symbol_name);
    TreeSitNode_setSpan(node, span);
    TreeSitNode_setParent(node, parent_idx);
    TreeSitNode_setFirst_child(node, -1);
    TreeSitNode_setNext_sibling(node, -1);
    TreeSitNode_setPrev_sibling(node, -1);
    TreeSitNode_setChild_count(node, 0);
    TreeSitNode_setPayload(node, nullptr);

    if (parent_idx < 0) {
        if (TreeSitTree_root(tree) < 0) {
            TreeSitTree_setRoot(tree, new_idx);
        }
    } else if ((u32) parent_idx < TreeSitTree_node_count(tree) - 1) {
        TreeSitNode *parent = &TreeSitTree_nodes(tree)[parent_idx];
        if (TreeSitNode_first_child(parent) < 0) {
            TreeSitNode_setFirst_child(parent, new_idx);
        } else {
            i32 curr = TreeSitNode_first_child(parent);
            while (TreeSitTree_nodes(tree)[curr].next_sibling >= 0) {
                curr = TreeSitTree_nodes(tree)[curr].next_sibling;
            }
            TreeSitTree_nodes(tree)[curr].next_sibling = new_idx;
            TreeSitNode_setPrev_sibling(node, curr);
        }
        TreeSitNode_setChild_count(parent, TreeSitNode_child_count(parent) + 1u);
    }

    return new_idx;
}

static bool walk_preorder_rec(
    const TreeSitTree *tree,
    i32 node_idx,
    i32 depth,
    TreeSitVisitorFn visitor,
    any user_data
) {
    if (node_idx < 0 || (u32) node_idx >= TreeSitTree_node_count(tree)) {
        return true;
    }

    if (!visitor(tree, node_idx, depth, user_data)) {
        return false;
    }

    i32 child = TreeSitTree_nodes(tree)[node_idx].first_child;
    while (child >= 0) {
        if (!walk_preorder_rec(tree, child, depth + 1, visitor, user_data)) {
            return false;
        }
        child = TreeSitTree_nodes(tree)[child].next_sibling;
    }
    return true;
}

bool TreeSitTree_walk_preorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data) {
    if (!tree || !visitor || TreeSitTree_root(tree) < 0) {
        return false;
    }
    return walk_preorder_rec(tree, TreeSitTree_root(tree), 0, visitor, user_data);
}

static bool walk_postorder_rec(
    const TreeSitTree *tree,
    i32 node_idx,
    i32 depth,
    TreeSitVisitorFn visitor,
    any user_data
) {
    if (node_idx < 0 || (u32) node_idx >= TreeSitTree_node_count(tree)) {
        return true;
    }

    i32 child = TreeSitTree_nodes(tree)[node_idx].first_child;
    while (child >= 0) {
        if (!walk_postorder_rec(tree, child, depth + 1, visitor, user_data)) {
            return false;
        }
        child = TreeSitTree_nodes(tree)[child].next_sibling;
    }

    return visitor(tree, node_idx, depth, user_data);
}

bool TreeSitTree_walk_postorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data) {
    if (!tree || !visitor || TreeSitTree_root(tree) < 0) {
        return false;
    }
    return walk_postorder_rec(tree, TreeSitTree_root(tree), 0, visitor, user_data);
}

i32 TreeSitTree_find_by_byte(const TreeSitTree *tree, u32 byte_offset) {
    if (!tree || TreeSitTree_root(tree) < 0) {
        return -1;
    }

    i32 curr = TreeSitTree_root(tree);
    i32 best_match = -1;

    while (curr >= 0) {
        const TreeSitNode *node = &TreeSitTree_nodes(tree)[curr];
        TreeSitSpan span = TreeSitNode_span(node);
        if (byte_offset >= span.start_byte && byte_offset <= span.end_byte) {
            best_match = curr;
            curr = TreeSitNode_first_child(node); // Drill down into finer-grained child
        } else {
            curr = TreeSitNode_next_sibling(node);
        }
    }

    return best_match;
}

u32 TreeSitTree_count_kind(const TreeSitTree *tree, u16 kind) {
    if (!tree) {
        return 0;
    }
    u32 count = 0;
    for (u32 i = 0; i < TreeSitTree_node_count(tree); i++) {
        if (TreeSitTree_nodes(tree)[i].kind == kind) {
            count++;
        }
    }
    return count;
}
