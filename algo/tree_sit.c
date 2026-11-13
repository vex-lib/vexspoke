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

;;INTENTION("AST node graph representation and hierarchy traversal")

TreeSitTree TreeSitTree_0(void) {
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

void TreeSitTree_destroy(TreeSitTree *tree) {
    if (!tree) {
        return;
    }
    if ((*tree).nodes) {
        free((*tree).nodes);
        (*tree).nodes = nullptr;
    }
    (*tree).node_count = 0;
    (*tree).node_capacity = 0;
    (*tree).root = -1;
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

    if ((*tree).node_count >= (*tree).node_capacity) {
        u32 new_cap = (*tree).node_capacity * 2;
        TreeSitNode *new_nodes = (TreeSitNode*) realloc((*tree).nodes, new_cap * sizeof(TreeSitNode));
        if (!new_nodes) {
            return -1;
        }
        (*tree).nodes = new_nodes;
        (*tree).node_capacity = new_cap;
    }

    i32 new_idx = (i32) (*tree).node_count++;
    TreeSitNode *node = &(*tree).nodes[new_idx];

    (*node).id = (u32) new_idx;
    (*node).kind = kind;
    (*node).symbol_name = symbol_name;
    (*node).span = span;
    (*node).parent = parent_idx;
    (*node).first_child = -1;
    (*node).next_sibling = -1;
    (*node).prev_sibling = -1;
    (*node).child_count = 0;
    (*node).payload = nullptr;

    if (parent_idx < 0) {
        if ((*tree).root < 0) {
            (*tree).root = new_idx;
        }
    } else if ((u32) parent_idx < (*tree).node_count - 1) {
        TreeSitNode *parent = &(*tree).nodes[parent_idx];
        if ((*parent).first_child < 0) {
            (*parent).first_child = new_idx;
        } else {
            i32 curr = (*parent).first_child;
            while ((*tree).nodes[curr].next_sibling >= 0) {
                curr = (*tree).nodes[curr].next_sibling;
            }
            (*tree).nodes[curr].next_sibling = new_idx;
            (*node).prev_sibling = curr;
        }
        (*parent).child_count++;
    }

    return new_idx;
}

static bool walk_preorder_rec(
    const TreeSitTree *tree,
    i32 node_idx,
    int depth,
    TreeSitVisitorFn visitor,
    any user_data
) {
    if (node_idx < 0 || (u32) node_idx >= (*tree).node_count) {
        return true;
    }

    if (!visitor(tree, node_idx, depth, user_data)) {
        return false;
    }

    i32 child = (*tree).nodes[node_idx].first_child;
    while (child >= 0) {
        if (!walk_preorder_rec(tree, child, depth + 1, visitor, user_data)) {
            return false;
        }
        child = (*tree).nodes[child].next_sibling;
    }
    return true;
}

bool TreeSitTree_walk_preorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data) {
    if (!tree || !visitor || (*tree).root < 0) {
        return false;
    }
    return walk_preorder_rec(tree, (*tree).root, 0, visitor, user_data);
}

static bool walk_postorder_rec(
    const TreeSitTree *tree,
    i32 node_idx,
    int depth,
    TreeSitVisitorFn visitor,
    any user_data
) {
    if (node_idx < 0 || (u32) node_idx >= (*tree).node_count) {
        return true;
    }

    i32 child = (*tree).nodes[node_idx].first_child;
    while (child >= 0) {
        if (!walk_postorder_rec(tree, child, depth + 1, visitor, user_data)) {
            return false;
        }
        child = (*tree).nodes[child].next_sibling;
    }

    return visitor(tree, node_idx, depth, user_data);
}

bool TreeSitTree_walk_postorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data) {
    if (!tree || !visitor || (*tree).root < 0) {
        return false;
    }
    return walk_postorder_rec(tree, (*tree).root, 0, visitor, user_data);
}

i32 TreeSitTree_find_by_byte(const TreeSitTree *tree, u32 byte_offset) {
    if (!tree || (*tree).root < 0) {
        return -1;
    }

    i32 curr = (*tree).root;
    i32 best_match = -1;

    while (curr >= 0) {
        const TreeSitNode *node = &(*tree).nodes[curr];
        TreeSitSpan span = (*node).span;
        if (byte_offset >= span.start_byte && byte_offset <= span.end_byte) {
            best_match = curr;
            curr = (*node).first_child; // Drill down into finer-grained child
        } else {
            curr = (*node).next_sibling;
        }
    }

    return best_match;
}

u32 TreeSitTree_count_kind(const TreeSitTree *tree, u16 kind) {
    if (!tree) {
        return 0;
    }
    u32 count = 0;
    for (u32 i = 0; i < (*tree).node_count; i++) {
        if ((*tree).nodes[i].kind == kind) {
            count++;
        }
    }
    return count;
}
