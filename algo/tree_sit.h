#ifndef VEXSPOKE_ALGO_TREE_SIT_H
#define VEXSPOKE_ALGO_TREE_SIT_H

#include "vex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TreeSitSpan {
    u32 start_byte;
    u32 end_byte;
    u32 start_row;
    u32 start_col;
    u32 end_row;
    u32 end_col;
} TreeSitSpan;

static inline TreeSitSpan TreeSitSpan_6(u32 startByte, u32 endByte,
                                        u32 startRow, u32 startColumn,
                                        u32 endRow, u32 endColumn) {
    return (TreeSitSpan) { .start_byte = startByte, .end_byte = endByte,
        .start_row = startRow, .start_col = startColumn,
        .end_row = endRow, .end_col = endColumn };
}
#define TreeSitSpan(...) CONSTRUCTOR_DISPATCH(TreeSitSpan, __VA_ARGS__)

typedef struct TreeSitNode {
    u32 id;
    u16 kind;
    const char *symbol_name;
    TreeSitSpan span;
    i32 parent;
    i32 first_child;
    i32 next_sibling;
    i32 prev_sibling;
    u32 child_count;
    any payload;
} TreeSitNode;

typedef struct TreeSitTree {
    TreeSitNode *nodes;
    u32 node_count;
    u32 node_capacity;
    i32 root;
} TreeSitTree;

typedef bool (*TreeSitVisitorFn)(const TreeSitTree *tree, i32 node_idx, int depth, any user_data);

TreeSitTree TreeSitTree_0();
TreeSitTree TreeSitTree_1(u32 initial_capacity);
#define TreeSitTree(...) CONSTRUCTOR_DISPATCH(TreeSitTree, __VA_ARGS__)
fn TreeSitTree_destroy(TreeSitTree *tree);
i32 TreeSitTree_add_node(
    i32 parent_idx,
    u16 kind,
    const char *symbol_name,
    TreeSitSpan span,
    TreeSitTree *tree
);
bool TreeSitTree_walk_preorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data);
bool TreeSitTree_walk_postorder(const TreeSitTree *tree, TreeSitVisitorFn visitor, any user_data);
i32 TreeSitTree_find_by_byte(const TreeSitTree *tree, u32 byte_offset);
u32 TreeSitTree_count_kind(const TreeSitTree *tree, u16 kind);

#ifdef __cplusplus
}
#endif

#endif // VEXSPOKE_ALGO_TREE_SIT_H
