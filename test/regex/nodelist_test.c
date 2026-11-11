// vexspoke/test/regex/nodelist_test.c — owner test for regex/nodelist.
//
// Proves the flat AST array: init leaves it empty, add appends zeroed nodes and
// returns increasing indices, growth past the seed capacity keeps earlier
// indices addressing the same nodes, out-of-range access returns nullptr, and
// free is null-safe and reusable.

#include <stddef.h>
#include <stdio.h>

#include "regex/nodelist.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks the empty list, index assignment, and the zeroed node payload.
static void testAdd(void) {
    RegexNodeList list = RegexNodeList();
    CHECK(list.count == 0);
    CHECK(list.nodes == nullptr);

    u32 a = RegexNodeList_add(&list, REGEX_NODE_CHAR);
    u32 b = RegexNodeList_add(&list, REGEX_NODE_CONCAT);
    CHECK(a == 0);
    CHECK(b == 1);
    CHECK(list.count == 2);

    RegexNode *first = RegexNodeList_get(&list, a);
    CHECK(first != nullptr);
    CHECK((*first).kind == REGEX_NODE_CHAR);
    // A freshly added node is fully zeroed apart from its kind.
    CHECK((*first).byte == 0);
    CHECK((*first).a == 0 && (*first).b == 0);
    CHECK((*first).repMin == 0 && (*first).repMax == 0);
    CHECK((*first).group == 0 && (*first).clsIndex == 0);

    RegexNode *second = RegexNodeList_get(&list, b);
    CHECK(second != nullptr);
    CHECK((*second).kind == REGEX_NODE_CONCAT);

    RegexNodeList_free(&list);
}

// Checks growth past the seed capacity keeps indices stable, and range checks.
static void testGrowth(void) {
    RegexNodeList list = RegexNodeList();

    // Past the 64-node seed capacity so the block is reallocated at least twice.
    enum { TOTAL = 300 };
    for (u32 i = 0; i < TOTAL; i++) {
        u32 index = RegexNodeList_add(&list, REGEX_NODE_CHAR);
        CHECK(index == i);
        RegexNode *node = RegexNodeList_get(&list, index);
        CHECK(node != nullptr);
        (*node).byte = (u8) (i & 0xFFu);
    }
    CHECK(list.count == TOTAL);
    CHECK(list.capacity >= TOTAL);

    // Index-addressed access still finds the right node after reallocation.
    for (u32 i = 0; i < TOTAL; i++) {
        const RegexNode *node = RegexNodeList_at(&list, i);
        CHECK(node != nullptr);
        CHECK((*node).byte == (u8) (i & 0xFFu));
    }

    CHECK(RegexNodeList_get(&list, TOTAL) == nullptr);
    CHECK(RegexNodeList_at(&list, TOTAL) == nullptr);
    CHECK(RegexNodeList_get(&list, REGEX_NODE_NONE) == nullptr);

    RegexNodeList_free(&list);
}

// Checks that free releases and re-zeroes the list, and is null-safe.
static void testFreeAndReuse(void) {
    RegexNodeList list = RegexNodeList();
    CHECK(RegexNodeList_add(&list, REGEX_NODE_ANY) == 0);
    RegexNodeList_free(&list);
    CHECK(list.nodes == nullptr);
    CHECK(list.count == 0);
    CHECK(list.capacity == 0);

    // A freed list is reusable from empty.
    CHECK(RegexNodeList_add(&list, REGEX_NODE_BOL) == 0);
    RegexNodeList_free(&list);
    RegexNodeList_free(&list); // d64 free must be a no-op
    CHECK(list.nodes == nullptr);
}

int main(void) {
    testAdd();
    testGrowth();
    testFreeAndReuse();

    if (g_failures == 0) {
        printf("nodelist_test: all assertions held\n");
        return 0;
    }
    printf("nodelist_test: %d FAILURES\n", g_failures);
    return 1;
}
