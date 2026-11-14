// regex/nodelist.c — the RegexNodeList flat AST array.

#include "vex.h"
#include "regex/nodelist.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RegexNodeList
 * ============================================================================
 * The flat AST array a parse produces. A regular expression's tree is built
 * bottom-up while scanning left to right, and its shape is only known once the
 * scan is done, so the tree cannot live in a fixed block: this list grows in
 * the engine arena and stores children as INDICES rather than pointers. That
 * is the whole reason the type exists — a reallocation moves the node block,
 * and an index into it survives the move where a pointer would not.
 *
 * It owns one arena block stamped TYPE_REGEX. Growth f64s from a named
 * default and is cold (compile time); a refused allocation returns
 * REGEX_NODE_NONE and leaves the existing nodes intact, so a failed compile
 * never leaves a half-linked tree (the Failure Atomicity and Recovery Law).
 * The list is single-threaded by construction: only the owning parser writes
 * it, and only that compile call reads it.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RegexNodeList (regex/nodelist.c — defined in regex/nodelist.h)
 * ============================================================================
 * A growable, index-addressed AST node array in the engine arena.
 *
 * STRUCT FIELDS (Mirroring regex/nodelist.h):
 * ----------------------------------------------------------------------------
 *   RegexNodeList {
 *     RegexNode *nodes;    // engine-arena block of `capacity` nodes
 *     u32   count;    // nodes in use
 *     u32   capacity; // nodes allocated
 *   }
 *
 * SLOT RECORD (owned here, behaviorless):
 * ----------------------------------------------------------------------------
 *   RegexNode {
 *     u8 kind; u8 byte; u16 reserved;
 *     u32 a; u32 b.sh; u32 repMin; u32 repMax;
 *     u32 group; u32 clsIndex;
 *   }
 *     a/b.sh are child indices, group is the capture number, clsIndex names a
 *     class bitmap; the record carries no behavior of its own.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - RegexNodeList() creates an empty list value
 *   - RegexNodeList_add(list, kind)
 *   - RegexNodeList_get(list, index)
 *   - RegexNodeList_at(list, index)
 *   - RegexNodeList_free(list)
 *
 * Private Core Functions: (.c static)
 *   - nodeGrow(block, bytes)
 *   - growToOneMore(list)
 *
 * Admission, ownership, failure:
 *   - init zeroes the list; add grows the arena block on demand and returns
 *     REGEX_NODE_NONE when the arena refuses (existing nodes preserved).
 *   - get/at are borrowed views valid until the next add or free; at accepts a
 *     const list, get does not.
 *   - free releases the block and zeroes the list; null-safe.
 * ============================================================================
 */

#define REGEX_NODELIST_SEED_CAPACITY 64u

static any nodeGrow(any block, bytes bytes) {
    if (block)
        return Memory_realloc(block, bytes);
    return Memory_alloc(TYPE_REGEX, bytes);
}

// Guarantees room for one more node; false when the arena refuses.
static bool growToOneMore(RegexNodeList *list) {
    if ((*list).count < (*list).capacity)
        return true;
    u32 next = (*list).capacity ? (*list).capacity * 2u : REGEX_NODELIST_SEED_CAPACITY;
    RegexNode *grown = (RegexNode*) nodeGrow((*list).nodes, (bytes) next * sizeof(RegexNode));
    if (!grown)
        return false;
    (*list).nodes = grown;
    (*list).capacity = next;
    return true;
}

RegexNodeList RegexNodeList_0() {
    RegexNodeList list;
    list.nodes = nullptr;
    list.count = 0u;
    list.capacity = 0u;
    return list;
}

u32 RegexNodeList_add(RegexNodeList *list, u8 kind) {
    if (!growToOneMore(list))
        return REGEX_NODE_NONE;
    RegexNode *node = &(*list).nodes[(*list).count];
    memset(node, 0, sizeof(RegexNode));
    (*node).kind = kind;
    return (*list).count++;
}

RegexNode *RegexNodeList_get(RegexNodeList *list, u32 index) {
    if (index >= (*list).count)
        return nullptr;
    return &(*list).nodes[index];
}

const RegexNode *RegexNodeList_at(const RegexNodeList *list, u32 index) {
    if (index >= (*list).count)
        return nullptr;
    return &(*list).nodes[index];
}

fn RegexNodeList_free(RegexNodeList *list) {
    if ((*list).nodes)
        Memory_free((*list).nodes);
    *list = RegexNodeList();
}
