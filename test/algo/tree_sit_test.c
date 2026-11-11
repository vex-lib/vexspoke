// vexspoke/test/algo/tree_sit_test.c — owner test for algo/tree_sit.
//
// Proves the flat AST graph:
//   - init clamps a small capacity up to 16, refuses nullptr; destroy is
//     null-safe and repeatable; add_node grows the array past capacity;
//   - parent/child/sibling linking: first_child chain, next/prev siblings,
//     child_count, and the root;
//   - a second root request (parent < 0 when a root exists) does not steal the
//     root, and an out-of-range parent index leaves the node unlinked;
//   - pre-order and post-order walks visit in the exact documented order with
//     correct depths; a visitor returning false stops the walk and the walk
//     returns false;
//   - find_by_byte drills to the finest containing node, and returns -1 when
//     no span contains the offset;
//   - count_kind counts matches across the whole flat array;
//   - walk/find/count are safe on an empty or destroyed tree and on nullptr.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "algo/tree_sit.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

typedef struct Trace {
    i32 ids[64];
    int     depths[64];
    bytes  count;
    int     stop_after;
} Trace;

/** Records visited node indices and depths into the test's bounded observation buffer. */
static bool record(const TreeSitTree *tree, i32 node_idx, int depth, void *user_data) {
    (void) tree;
    Trace *t = (Trace*) user_data;
    if ((*t).count < 64) {
        (*t).ids[(*t).count] = node_idx;
        (*t).depths[(*t).count] = depth;
    }
    (*t).count++;
    if ((*t).stop_after > 0 && (int) (*t).count >= (*t).stop_after)
        return false;
    return true;
}

/** Constructs a half-open test span from its start and end offsets. */
static TreeSitSpan span(u32 s, u32 e) {
    TreeSitSpan sp = { s, e, 0, 0, 0, 0 };
    return sp;
}

// Builds: root(0)->A(1)->g(4); root->B(2); root->C(3).
// Returns the tree root index via the tree struct held by the caller.
/** Populates a small deterministic tree used by the traversal and lookup cases. */
static void build(TreeSitTree *tree) {
    CHECK(TreeSitTree_add_node(-1, 1, "root", span(0, 100), tree) == 0);
    CHECK(TreeSitTree_add_node(0, 2, "A", span(0, 30), tree) == 1);
    CHECK(TreeSitTree_add_node(0, 2, "B", span(40, 60), tree) == 2);
    CHECK(TreeSitTree_add_node(0, 2, "C", span(70, 100), tree) == 3);
    CHECK(TreeSitTree_add_node(1, 3, "g", span(5, 10), tree) == 4);
}

/** Verifies TreeSit initialization and teardown on a minimally populated tree. */
static void test_lifecycle(void) {
    TreeSitTree defaultTree = TreeSitTree();
    CHECK(defaultTree.nodes != nullptr);
    CHECK(defaultTree.node_capacity == 16u);
    TreeSitTree_destroy(&defaultTree);
    TreeSitTree t = TreeSitTree(0u);                // clamped to 16
    CHECK(t.nodes != nullptr);
    CHECK(t.node_capacity == 16);
    CHECK(t.node_count == 0);
    CHECK(t.root == -1);
    CHECK(TreeSitTree_add_node(-1, 1, "r", span(0, 0), nullptr) == -1);
    TreeSitTree_destroy(&t);
    TreeSitTree_destroy(&t);                        // safe twice
    TreeSitTree_destroy(nullptr);                   // safe

    // Grow past the initial capacity.
    t = TreeSitTree(16u); CHECK(t.nodes != nullptr);
    for (int i = 0; i < 100; i++)
        CHECK(TreeSitTree_add_node(i == 0 ? -1 : (i - 1), 7, "n", span(0, 1), &t) == i);
    CHECK(t.node_count == 100);
    CHECK(t.node_capacity >= 100);
    CHECK(TreeSitTree_count_kind(&t, 7) == 100);
    TreeSitTree_destroy(&t);
}

/** Checks parent/child and sibling links remain consistent as nodes are connected. */
static void test_linking(void) {
    TreeSitTree t;
    t = TreeSitTree(16u); CHECK(t.nodes != nullptr);
    build(&t);

    TreeSitNode *root = &t.nodes[0];
    TreeSitNode *a = &t.nodes[1];
    TreeSitNode *b = &t.nodes[2];
    TreeSitNode *c = &t.nodes[3];
    TreeSitNode *g = &t.nodes[4];

    CHECK(t.root == 0);
    CHECK((*root).child_count == 3);
    CHECK((*root).first_child == 1);
    CHECK((*a).next_sibling == 2);
    CHECK((*b).prev_sibling == 1);
    CHECK((*b).next_sibling == 3);
    CHECK((*c).prev_sibling == 2);
    CHECK((*c).next_sibling == -1);
    CHECK((*a).child_count == 1);
    CHECK((*a).first_child == 4);
    CHECK((*g).parent == 1);
    CHECK((*g).child_count == 0);

    // A second root request does not replace the root or link anything.
    CHECK(TreeSitTree_add_node(-1, 9, "orphan", span(200, 210), &t) == 5);
    CHECK(t.root == 0);
    CHECK((*root).child_count == 3);
    CHECK(TreeSitTree_count_kind(&t, 9) == 1);

    // An out-of-range parent (>= new index) leaves the node unlinked; walking
    // never reaches it.
    CHECK(TreeSitTree_add_node(99, 8, "f32ing", span(0, 5), &t) == 6);

    Trace tr = { 0 };
    CHECK(TreeSitTree_walk_preorder(&t, record, &tr));
    // 0 1 4 2 3  (the orphan/unlinked nodes are outside the root subtree)
    CHECK(tr.count == 5);
    CHECK(tr.ids[0] == 0 && tr.ids[1] == 1 && tr.ids[2] == 4 &&
          tr.ids[3] == 2 && tr.ids[4] == 3);
    CHECK(tr.depths[0] == 0 && tr.depths[1] == 1 && tr.depths[2] == 2 &&
          tr.depths[3] == 1 && tr.depths[4] == 1);

    TreeSitTree_destroy(&t);
}

/** Exercises the supported traversal orders and validates recorded node visits. */
static void test_walks(void) {
    TreeSitTree t;
    t = TreeSitTree(16u); CHECK(t.nodes != nullptr);
    build(&t);

    Trace pre = { 0 };
    CHECK(TreeSitTree_walk_preorder(&t, record, &pre));
    CHECK(pre.count == 5);
    CHECK(pre.ids[0] == 0 && pre.ids[1] == 1 && pre.ids[2] == 4 &&
          pre.ids[3] == 2 && pre.ids[4] == 3);

    Trace post = { 0 };
    CHECK(TreeSitTree_walk_postorder(&t, record, &post));
    CHECK(post.count == 5);
    CHECK(post.ids[0] == 4 && post.ids[1] == 1 && post.ids[2] == 2 &&
          post.ids[3] == 3 && post.ids[4] == 0);

    // Visitor early-stop propagates false.
    Trace halt = { 0 };
    halt.stop_after = 2;
    CHECK(!TreeSitTree_walk_preorder(&t, record, &halt));
    CHECK(halt.count == 2);

    Trace haltPost = { 0 };
    haltPost.stop_after = 2;
    CHECK(!TreeSitTree_walk_postorder(&t, record, &haltPost));
    CHECK(haltPost.count == 2 && haltPost.ids[0] == 4 && haltPost.ids[1] == 1);

    // Null / empty refusals.
    CHECK(!TreeSitTree_walk_preorder(nullptr, record, &pre));
    CHECK(!TreeSitTree_walk_preorder(&t, nullptr, &pre));
    CHECK(!TreeSitTree_walk_postorder(nullptr, record, &pre));
    CHECK(!TreeSitTree_walk_postorder(&t, nullptr, &pre));

    TreeSitTree empty;
    empty = TreeSitTree(16u); CHECK(empty.nodes != nullptr);
    Trace e = { 0 };
    CHECK(!TreeSitTree_walk_preorder(&empty, record, &e));   // no root
    CHECK(!TreeSitTree_walk_postorder(&empty, record, &e));
    TreeSitTree_destroy(&empty);

    TreeSitTree_destroy(&t);
}

/** Verifies span lookup and subtree counting against the deterministic fixture. */
static void test_find_and_count(void) {
    TreeSitTree t;
    t = TreeSitTree(16u); CHECK(t.nodes != nullptr);
    build(&t);

    CHECK(TreeSitTree_find_by_byte(&t, 7) == 4);    // finest span 5..10
    CHECK(TreeSitTree_find_by_byte(&t, 45) == 2);   // inside B 40..60
    CHECK(TreeSitTree_find_by_byte(&t, 90) == 3);   // inside C 70..100
    CHECK(TreeSitTree_find_by_byte(&t, 0) == 1);    // root->A contains 0; g does not
    CHECK(TreeSitTree_find_by_byte(&t, 35) == 0);   // gap between A and B -> root
    CHECK(TreeSitTree_find_by_byte(&t, 200) == -1); // outside root

    CHECK(TreeSitTree_count_kind(&t, 1) == 1);
    CHECK(TreeSitTree_count_kind(&t, 2) == 3);
    CHECK(TreeSitTree_count_kind(&t, 3) == 1);
    CHECK(TreeSitTree_count_kind(&t, 99) == 0);

    CHECK(TreeSitTree_find_by_byte(nullptr, 0) == -1);
    CHECK(TreeSitTree_count_kind(nullptr, 0) == 0);

    TreeSitTree_destroy(&t);
}

/** Runs the TreeSit owner scenarios and reports aggregate assertion status. */
int main(void) {
    test_lifecycle();
    test_linking();
    test_walks();
    test_find_and_count();

    if (g_failures == 0) {
        printf("tree_sit_test: all assertions held\n");
        return 0;
    }
    printf("tree_sit_test: %d FAILURES\n", g_failures);
    return 1;
}
