#include "vex.h"
#include <stdio.h>

#include "algo/segment_index.h"
#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "substrate/variable/variable_hash_map.h"
#include "substrate/variable/variable_slot.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: SegmentIndexTest (segment_index_test/test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for the positional segment index: the '.' splitter, index
 * build, the ranked phrase search (exact > suffix > contains, then start /
 * length / lexicographic), truncation, the VariableHashMap bridge, and
 * null-safety.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main()
 * ============================================================================
 */

static i32 g_fail = 0;
static i32 g_checks = 0;

;;GETTER
static inline const char *SegmentEntry_name(const SegmentEntry *s) { return (*s).name; }

;;GETTER
static inline ChunkedList * SegmentIndex_entries(const SegmentIndex *s) { return (*s).entries; }

;;GETTER
static inline u32 SegmentMatch_entry(const SegmentMatch *s) { return (*s).entry; }

#define CHECK(cond)                                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_fail++;                                                     \
            printf("  FAIL line %d: %s\n", __LINE__, #cond);              \
        }                                                                 \
    } while (0)

// Fetch the name of the entry a match points at (via the index's entry row).
/** Resolves a match's stored field index to its fixture name for readable assertions. */
static const char *matchName(const SegmentIndex *index, const SegmentMatch *m) {
    const SegmentEntry *e = (const SegmentEntry*) ChunkedList_slot(SegmentIndex_entries(index), SegmentMatch_entry(m));
    return e ? SegmentEntry_name(e) : "";
}

/** Adds the deterministic field fixture used by segment matching scenarios. */
static fn seed(SegmentIndex *index) {
    SegmentIndex_add(index, "a.b.c", 1u);
    SegmentIndex_add(index, "b.c", 2u);
    SegmentIndex_add(index, "x.b.c", 3u);
    SegmentIndex_add(index, "b.c.d", 4u);
    SegmentIndex_add(index, "position.x", 5u);
    SegmentIndex_add(index, "position.z", 6u);
    SegmentIndex_add(index, "vector.x", 7u);
    SegmentIndex_add(index, "vector.y", 8u);
    SegmentIndex_add(index, "health", 9u);
}

/** Checks name segmentation for ordinary, dotted, empty, and malformed names. */
static fn testSplitter() {
    printf("[1] the '.' splitter\n");

    CHECK(SegmentIndex_segmentCount("health") == 1u);
    CHECK(SegmentIndex_segmentCount("character.position") == 2u);
    CHECK(SegmentIndex_segmentCount("a.b.c.d") == 4u);
    CHECK(SegmentIndex_segmentCount(nullptr) == 0u);
    CHECK(SegmentIndex_segmentCount("") == 0u);
    CHECK(SegmentIndex_segmentCount(".") == 0u);
    CHECK(SegmentIndex_segmentCount("a.") == 0u);
    CHECK(SegmentIndex_segmentCount(".a") == 0u);
    CHECK(SegmentIndex_segmentCount("a..b") == 0u);
    CHECK(SegmentIndex_segmentCount("abcdefghijklmnopqrstuvwx") == 0u); // 24
    CHECK(SegmentIndex_segmentCount("abcdefghijklmnopqrstuvw") == 1u);  // 23

    char out[SEGMENT_NAME_BYTES];
    CHECK(SegmentIndex_segment("a.b.c", 0u, out, sizeof(out)) == 1 && strcmp(out, "a") == 0);
    CHECK(SegmentIndex_segment("a.b.c", 2u, out, sizeof(out)) == 1 && strcmp(out, "c") == 0);
    CHECK(SegmentIndex_segment("panel.fill", 1u, out, sizeof(out)) == 4 && strcmp(out, "fill") == 0);
    CHECK(SegmentIndex_segment("a.b.c", 3u, out, sizeof(out)) == -1);
    CHECK(SegmentIndex_segment("a.b.c", 0u, out, 0u) == -1);
    CHECK(SegmentIndex_segment(nullptr, 0u, out, sizeof(out)) == -1);
}

/** Verifies index construction and lookup of the seeded field names. */
static fn testBuild() {
    printf("[2] index build + getters + null-safety\n");

    SegmentIndex *index = SegmentIndex_0();
    CHECK(index != nullptr);
    if (!index)
        return;
    CHECK(Memory_type(index) == TYPE_SEGMENT_INDEX);
    CHECK(SegmentIndex_count(index) == 0u);
    CHECK(SegmentIndex_isEmpty(index) == true);

    seed(index);
    CHECK(SegmentIndex_count(index) == 9u);
    CHECK(SegmentIndex_isEmpty(index) == false);
    CHECK(SegmentIndex_add(index, "bad name", 0u) == SEGMENT_ENTRY_NONE); // space
    CHECK(SegmentIndex_add(index, "x.y.z", 0u) == 9u);                    // next id
    CHECK(SegmentIndex_count(index) == 10u);

    SegmentIndex_clear(index);
    CHECK(SegmentIndex_count(index) == 0u);

    SegmentIndex_free(index);
    SegmentIndex_free(nullptr);
    CHECK(true);
}

/** Checks candidate ranking prefers the most specific matching field path. */
static fn testRanking() {
    printf("[3] ranked phrase search: exact > suffix > contains\n");

    SegmentIndex *index = SegmentIndex_0();
    if (!index) {
        CHECK(false);
        return;
    }
    seed(index);

    SegmentMatch out[8];
    bool truncated = true;

    // "b.c": exact "b.c"; suffix "a.b.c","x.b.c" (start 1, len 5, lex); contains "b.c.d".
    bytes n = SegmentIndex_query(index, "b.c", out, 8u, &truncated);
    CHECK(n == 4u && truncated == false);
    CHECK(strcmp(matchName(index, &out[0]), "b.c") == 0 && out[0].tier == SEGMENT_MATCH_EXACT);
    CHECK(strcmp(matchName(index, &out[1]), "a.b.c") == 0 && out[1].tier == SEGMENT_MATCH_SUFFIX);
    CHECK(strcmp(matchName(index, &out[2]), "x.b.c") == 0 && out[2].tier == SEGMENT_MATCH_SUFFIX);
    CHECK(strcmp(matchName(index, &out[3]), "b.c.d") == 0 && out[3].tier == SEGMENT_MATCH_CONTAINS);

    // "c": suffix "b.c"(start1), then start2 tie broken by lex ("a.b.c" < "x.b.c"), then contains.
    n = SegmentIndex_query(index, "c", out, 8u, &truncated);
    CHECK(n == 4u);
    CHECK(strcmp(matchName(index, &out[0]), "b.c") == 0);
    CHECK(strcmp(matchName(index, &out[1]), "a.b.c") == 0);
    CHECK(strcmp(matchName(index, &out[2]), "x.b.c") == 0);
    CHECK(strcmp(matchName(index, &out[3]), "b.c.d") == 0);

    // "x" is broad: suffix hits first ("vector.x" before "position.x"), then "x.b.c".
    n = SegmentIndex_query(index, "x", out, 8u, &truncated);
    CHECK(n == 3u);
    CHECK(strcmp(matchName(index, &out[0]), "vector.x") == 0);
    CHECK(strcmp(matchName(index, &out[1]), "position.x") == 0);
    CHECK(strcmp(matchName(index, &out[2]), "x.b.c") == 0);

    // Exact multi-segment.
    n = SegmentIndex_query(index, "position.x", out, 8u, &truncated);
    CHECK(n == 1u && strcmp(matchName(index, &out[0]), "position.x") == 0 && out[0].tier == SEGMENT_MATCH_EXACT);

    // Contiguous middle.
    n = SegmentIndex_query(index, "b.c", out, 8u, &truncated);
    CHECK(n == 4u);
    CHECK(out[3].start == 0u); // "b.c.d" begins at 0

    // Folding: uppercase query resolves.
    n = SegmentIndex_query(index, "POSITION.X", out, 8u, &truncated);
    CHECK(n == 1u && strcmp(matchName(index, &out[0]), "position.x") == 0);

    // Miss: an absent segment yields nothing.
    CHECK(SegmentIndex_query(index, "nope", out, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, "position.nope", out, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, "", out, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, "a..b", out, 8u, &truncated) == 0u);

    // Truncation: keep the best cap, report the true total.
    n = SegmentIndex_query(index, "b.c", out, 2u, &truncated);
    CHECK(n == 4u && truncated == true);
    CHECK(strcmp(matchName(index, &out[0]), "b.c") == 0);
    CHECK(strcmp(matchName(index, &out[1]), "a.b.c") == 0);

    // Null-safety.
    CHECK(SegmentIndex_query(nullptr, "b.c", out, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, nullptr, out, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, "b.c", nullptr, 8u, &truncated) == 0u);
    CHECK(SegmentIndex_query(index, "b.c", out, 0u, &truncated) == 0u);

    SegmentIndex_free(index);
}

/** Exercises the mini-map bridge and confirms its matches resolve to source fields. */
static fn testBridge() {
    printf("[4] buildFrom(VariableHashMap): payload is the VariableSlot*\n");

    VariableHashMap *map = VariableHashMap();
    CHECK(map != nullptr);
    if (!map)
        return;
    VariableHashMap_add(map, "character.position.x", 0x1u);
    VariableHashMap_add(map, "position.x", 0x2u);
    VariableHashMap_add(map, "health", 0x3u);

    SegmentIndex *index = SegmentIndex_0();
    CHECK(index != nullptr);
    if (!index) {
        VariableHashMap_free(map);
        return;
    }
    CHECK(SegmentIndex_buildFrom(index, map) == 3u);
    CHECK(SegmentIndex_count(index) == 3u);

    SegmentMatch out[4];
    bool truncated = true;
    bytes n = SegmentIndex_query(index, "position.x", out, 4u, &truncated);
    CHECK(n == 2u && truncated == false);
    // Exact first (payload = its slot), then the suffix.
    VariableSlot *first = (VariableSlot*) out[0].payload;
    VariableSlot *second = (VariableSlot*) out[1].payload;
    CHECK(VariableSlot_getPointer(first) == 0x2u);
    CHECK(VariableSlot_getPointer(second) == 0x1u);

    // Rebuild is repeatable.
    CHECK(SegmentIndex_buildFrom(index, map) == 3u);
    CHECK(SegmentIndex_query(index, "position.x", out, 4u, &truncated) == 2u);

    // Strings.
    char buf[128];
    SegmentIndex_toString(index, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strcmp(buf, "SegmentIndex(entries=3)") == 0);
    SegmentIndex_toStringStruct(index, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strncmp(buf, "SegmentIndex { active=true, count=3,", 34) == 0);
    truncated = true;
    SegmentIndex_toString(nullptr, buf, sizeof(buf), &truncated);
    CHECK(truncated == false && strcmp(buf, "nullptr") == 0);

    SegmentIndex_free(index);
    VariableHashMap_free(map);
}

/** Runs the SegmentIndex owner scenarios and reports aggregate assertion status. */
i32 main() {
    printf("=== SegmentIndex (dotted-name search) Test Suite ===\n\n");

    testSplitter();
    testBuild();
    testRanking();
    testBridge();

    printf("\n%d checks, %d failed\n", g_checks, g_fail);
    if (g_fail == 0)
        printf("=== SegmentIndex Test Suite: ALL PASS ===\n");
    return g_fail == 0 ? 0 : 1;
}
