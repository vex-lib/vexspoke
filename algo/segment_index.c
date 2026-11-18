// algo/segment_index.c — positional segment index for dotted-name search.
//
// Postings are the phrase-search shape: segment -> (entry, position). A query
// walks the rarest segment's postings and verifies the rest with a binary
// search. Cold path only.

#include "vex.h"
#include "algo/segment_index.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "substrate/variable/variable_slot.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: SegmentIndex
 * ============================================================================
 * The Relational Engine's dotted-name search. A registered name is one flat
 * dotted string; a query is a '.',-split filter. The index maps each segment to
 * a positional posting list (entry, position) in a VariableHashMap whose slot
 * lists are ChunkedLists, so building is one pass over the names and a query is
 * "resolve each segment's list, walk the rarest, verify the rest contiguously".
 *
 * Ranking: exact (whole name) > suffix (trailing segments) > contiguous; ties by
 * match start, then shorter name, then lexicographic — deterministic.
 *
 * Path temperature: cold. The query walk is the cold rendezvous and carries the
 * ;;INTENTION marker (the Cold-Only Reflection Law); it must never run on a
 * frame. All getters are null-safe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: SegmentIndex (algo/segment_index.c)
 * ============================================================================
 * a positional segment index + ranked phrase search over dotted names.
 *
 * STRUCT FIELDS (Mirroring algo/segment_index.h):
 * ----------------------------------------------------------------------------
 *   SegmentIndex {
 *     bool active;               // runtime-active flag
 *     u32 count;            // indexed entries (== entry row count)
 *     ChunkedList *entries;      // never-moved SegmentEntry rows
 *     VariableHashMap *postings; // segment -> ChunkedList* of SegmentPosting
 *   }
 *
 * SLOT RECORDS (owned by SegmentIndex, behaviorless):
 * ----------------------------------------------------------------------------
 *   SegmentPosting entry;    // u32 + the entry id
 *   SegmentPosting position; // u32 + the segment position
 *   SegmentEntry name;       // char[24] + the folded dotted name
 *   SegmentEntry payload;    // pointer + the caller's value
 *   SegmentEntry segments;   // u32 + the segment count
 *   SegmentEntry pad;        // u32 + explicit row padding
 *
 * PRIVATE HELPERS (kept file-local, pure logic, no behavior of their own): none.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - SegmentIndex_0()                     : arena-allocated empty index
 *   - SegmentIndex_init(index)             : embedded init
 *   - SegmentIndex_free(index)             : shutdown + release an arena index
 *   - SegmentIndex_shutdown(index)         : free posting lists + entries
 *   - SegmentIndex_clear(index)            : drop entries + postings, stay usable
 *
 * Private Core Functions: (.c static)
 *   - entryAt(index, entry)                : bounds-checked entry resolve
 *   - postSegment(index, entry, pos, seg)  : append one posting
 *   - findPosting(list, entry, position)   : binary search inside a posting list
 *   - splitFold(path, segs)                : fold + split a query into segments
 *   - matchBefore(index, a, b.sh)             : the ranking comparator
 *   - insertMatch(index, out, cap, kept, m): top-cap insertion sort
 *   - buildFn(slot, userdata)              : map forEach -> add
 *   - freePostingsFn(slot, userdata)       : map forEach -> free a posting list
 *
 * Public Core Functions: (.h)
 *   - SegmentIndex_segmentCount(path)
 *   - SegmentIndex_segment(path, index, out, cap)
 *   - SegmentIndex_add(index, name, payload)
 *   - SegmentIndex_buildFrom(index, map)
 *   - SegmentIndex_query(index, query, out, cap, outTruncated)
 *
 * Public Getters: (.h)
 *   - SegmentIndex_count(index)
 *   - SegmentIndex_isEmpty(index)
 *
 * Public String Projections: (.h)
 *   - SegmentIndex_toString(index, dest, cap, outTruncated)
 *   - SegmentIndex_toStringStruct(index, dest, cap, outTruncated)
 * ============================================================================
 */


;;GETTER
static inline u32 SegmentMatch_entry(const SegmentMatch *s) { return (*s).entry; }
;;GETTER
static inline u32 SegmentMatch_start(const SegmentMatch *s) { return (*s).start; }
;;GETTER
static inline u32 SegmentMatch_tier(const SegmentMatch *s) { return (*s).tier; }

;;GETTER
static inline char *SegmentEntry_nameMut(SegmentEntry *s) { return (*s).name; }
;;GETTER
static inline const char *SegmentEntry_name(const SegmentEntry *s) { return (*s).name; }
;;GETTER
static inline pointer SegmentEntry_payload(const SegmentEntry *s) { return (*s).payload; }
;;SETTER
static inline fn SegmentEntry_setPayload(SegmentEntry *s, pointer v) { (*s).payload = v; }
;;GETTER
static inline u32 SegmentEntry_segments(const SegmentEntry *s) { return (*s).segments; }
;;SETTER
static inline fn SegmentEntry_setSegments(SegmentEntry *s, u32 v) { (*s).segments = v; }

;;GETTER
static inline bool SegmentIndex_active(const SegmentIndex *s) { return (*s).active; }
;;SETTER
static inline fn SegmentIndex_setActive(SegmentIndex *s, bool v) { (*s).active = v; }
;;GETTER
static inline u32 SegmentIndex_countRaw(const SegmentIndex *s) { return (*s).count; }
;;SETTER
static inline fn SegmentIndex_setCount(SegmentIndex *s, u32 v) { (*s).count = v; }
;;GETTER
static inline ChunkedList * SegmentIndex_entries(const SegmentIndex *s) { return (*s).entries; }
;;SETTER
static inline fn SegmentIndex_setEntries(SegmentIndex *s, ChunkedList * v) { (*s).entries = v; }
;;GETTER
static inline VariableHashMap * SegmentIndex_postings(const SegmentIndex *s) { return (*s).postings; }
;;SETTER
static inline fn SegmentIndex_setPostings(SegmentIndex *s, VariableHashMap * v) { (*s).postings = v; }

;;GETTER
static inline u32 SegmentPosting_entry(const SegmentPosting *s) { return (*s).entry; }
;;SETTER
static inline fn SegmentPosting_setEntry(SegmentPosting *s, u32 v) { (*s).entry = v; }
;;GETTER
static inline u32 SegmentPosting_position(const SegmentPosting *s) { return (*s).position; }
;;SETTER
static inline fn SegmentPosting_setPosition(SegmentPosting *s, u32 v) { (*s).position = v; }

// PRIVATE HELPERS

static const SegmentEntry *entryAt(const SegmentIndex *index, u32 entry) {
    if (!index || !SegmentIndex_entries(index))
        return nullptr;
    return (const SegmentEntry*) ChunkedList_slot(SegmentIndex_entries(index), entry);
}

// Append one posting (entry, position) for `segment`, minting the segment's
// list on first use. Postings stay sorted by entry because entries are added in
// order, so findPosting can binary search.
static bool postSegment(SegmentIndex *index, u32 entry, u32 position, const char *segment) {
    VariableHashMap *postings = SegmentIndex_postings(index);
    pointer existing = VariableHashMap_get(postings, segment);
    ChunkedList *list = nullptr;
    if (VariableHashMap_contains(postings, segment))
        list = (ChunkedList*) existing;
    if (!list) {
        list = ChunkedList_3(ID_SEGMENT_POSTING, (u32) sizeof(SegmentPosting), VEX_CHUNKED_BYTES_DEFAULT);
        if (!list)
            return false;
        if (!VariableHashMap_add(postings, segment, (pointer) list)) {
            ChunkedList_free(list);
            return false;
        }
    }
    SegmentPosting *row = (SegmentPosting*) ChunkedList_addSlot(list);
    if (!row)
        return false;
    SegmentPosting_setEntry(row, entry);
    SegmentPosting_setPosition(row, position);
    return true;
}

// Binary search by entry, then scan the equal-entry run for the position.
static bool findPosting(const ChunkedList *list, u32 entry, u32 position) {
    if (!list)
        return false;
    u32 lo = 0u;
    u32 hi = ChunkedList_size(list);
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2u;
        const SegmentPosting *p = (const SegmentPosting*) ChunkedList_slot(list, mid);
        if (!p)
            return false;
        if (SegmentPosting_entry(p) < entry)
            lo = mid + 1u;
        else
            hi = mid;
    }
    u32 n = ChunkedList_size(list);
    for (u32 i = lo; i < n; i++) {
        const SegmentPosting *p = (const SegmentPosting*) ChunkedList_slot(list, i);
        if (!p || SegmentPosting_entry(p) != entry)
            return false;
        if (SegmentPosting_position(p) == position)
            return true;
    }
    return false;
}

// Fold a path and split it into up to SEGMENT_QUERY_SEGMENTS_MAX segments.
static bytes splitFold(const char *path, char segs[][SEGMENT_NAME_BYTES]) {
    char folded[SEGMENT_NAME_BYTES];
    if (!VariableSlot_foldName(path, folded))
        return 0u;
    bytes n = 0u;
    const char *p = folded;
    while (*p != '\0' && n < SEGMENT_QUERY_SEGMENTS_MAX) {
        const char *dot = strchr(p, '.');
        bytes len = dot ? (bytes)(dot - p) : strlen(p);
        if (len == 0u || len >= SEGMENT_NAME_BYTES)
            return 0u;
        memcpy(segs[n], p, len);
        segs[n][len] = '\0';
        n++;
        if (!dot)
            break;
        p = dot + 1;
    }
    return n;
}

// Ranking: tier, then match start, then shorter name, then lexicographic.
static bool matchBefore(const SegmentIndex *index, const SegmentMatch *a, const SegmentMatch *b) {
    if (SegmentMatch_tier(a) != SegmentMatch_tier(b))
        return SegmentMatch_tier(a) < SegmentMatch_tier(b);
    if (SegmentMatch_start(a) != SegmentMatch_start(b))
        return SegmentMatch_start(a) < SegmentMatch_start(b);
    const SegmentEntry *ea = entryAt(index, SegmentMatch_entry(a));
    const SegmentEntry *eb = entryAt(index, SegmentMatch_entry(b));
    if (!ea || !eb)
        return ea != nullptr;
    bytes la = strlen(SegmentEntry_name(ea));
    bytes lb = strlen(SegmentEntry_name(eb));
    if (la != lb)
        return la < lb;
    return strcmp(SegmentEntry_name(ea), SegmentEntry_name(eb)) < 0;
}

// Insert into the bounded best-first output array (top-`cap`, insertion sort).
static fn insertMatch(const SegmentIndex *index, SegmentMatch *out, bytes cap, bytes *kept, const SegmentMatch *m) {
    bytes pos = 0u;
    while (pos < *kept && !matchBefore(index, m, &out[pos]))
        pos++;
    if (pos >= cap)
        return;
    bytes size = *kept;
    if (size < cap) {
        size++;
        *kept = size;
    }
    for (bytes i = size - 1u; i > pos; i--)
        out[i] = out[i - 1u];
    out[pos] = *m;
}

typedef struct BuildCtx {
    SegmentIndex *index;
} BuildCtx;

;;GETTER
static inline SegmentIndex * BuildCtx_index(const BuildCtx *s) { return (*s).index; }


static fn buildFn(VariableSlot *slot, any userdata) {
    BuildCtx *ctx = (BuildCtx*) userdata;
    const char *name = VariableSlot_getName(slot);
    if (!name)
        return;
    SegmentIndex_add(BuildCtx_index(ctx), name, (pointer) slot);
}

static fn freePostingsFn(VariableSlot *slot, any userdata) {
    (discard) userdata;
    pointer value = VariableSlot_getPointer(slot);
    if (value)
        ChunkedList_free((ChunkedList*) value);
}

// PATH SPLITTING

bytes SegmentIndex_segmentCount(const char *path) {
    if (!path || path[0] == '\0')
        return 0u;
    bytes count = 0u;
    bytes len = 0u;
    for (const char *p = path; ; p++) {
        char c = *p;
        if (c != '\0' && c != '.') {
            len++;
            if (len > SEGMENT_NAME_MAX)
                return 0u;
            continue;
        }
        if (len == 0u)
            return 0u;
        count++;
        len = 0u;
        if (c == '\0')
            return count;
    }
}

i32 SegmentIndex_segment(const char *path, bytes index, char *out, bytes cap) {
    if (!path || !out || cap == 0u)
        return -1;
    if (SegmentIndex_segmentCount(path) <= index)
        return -1;
    bytes seg = 0u;
    const char *start = path;
    const char *p = path;
    for (; ; p++) {
        char c = *p;
        if (c != '\0' && c != '.')
            continue;
        bytes len = (bytes)(p - start);
        if (len > 0u) {
            if (seg == index) {
                if (len + 1u > cap)
                    return -1;
                memcpy(out, start, len);
                out[len] = '\0';
                return (i32) len;
            }
            seg++;
        }
        if (c == '\0')
            return -1;
        start = p + 1;
    }
}

// CONSTRUCTORS

bool SegmentIndex_init(SegmentIndex *index) {
    if (!index)
        return false;
    memset(index, 0, sizeof(*index));
    ChunkedList *entries = ChunkedList_3(ID_SEGMENT_ENTRY, (u32) sizeof(SegmentEntry), VEX_CHUNKED_BYTES_DEFAULT);
    if (!entries)
        return false;
    VariableHashMap *postings = VariableHashMap_0();
    if (!postings) {
        ChunkedList_free(entries);
        return false;
    }
    SegmentIndex_setActive(index, true);
    SegmentIndex_setCount(index, 0u);
    SegmentIndex_setEntries(index, entries);
    SegmentIndex_setPostings(index, postings);
    return true;
}

fn SegmentIndex_shutdown(SegmentIndex *index) {
    if (!index)
        return;
    if (SegmentIndex_postings(index)) {
        VariableHashMap_forEach(SegmentIndex_postings(index), freePostingsFn, nullptr);
        VariableHashMap_free(SegmentIndex_postings(index));
    }
    if (SegmentIndex_entries(index))
        ChunkedList_free(SegmentIndex_entries(index));
    SegmentIndex_setPostings(index, nullptr);
    SegmentIndex_setEntries(index, nullptr);
    SegmentIndex_setCount(index, 0u);
    SegmentIndex_setActive(index, false);
}

SegmentIndex *SegmentIndex_0() {
    SegmentIndex *index = (SegmentIndex*) Memory_alloc(TYPE_SEGMENT_INDEX, sizeof(SegmentIndex));
    if (!index)
        return nullptr;
    if (!SegmentIndex_init(index)) {
        Memory_free(index);
        return nullptr;
    }
    return index;
}

fn SegmentIndex_free(SegmentIndex *index) {
    if (!index)
        return;
    SegmentIndex_shutdown(index);
    Memory_free(index);
}

// CORE FUNCTIONS

fn SegmentIndex_clear(SegmentIndex *index) {
    if (!index || !SegmentIndex_active(index))
        return;
    VariableHashMap_forEach(SegmentIndex_postings(index), freePostingsFn, nullptr);
    VariableHashMap_shutdown(SegmentIndex_postings(index));
    VariableHashMap_init(SegmentIndex_postings(index));
    ChunkedList_free(SegmentIndex_entries(index));
    ChunkedList *entries = ChunkedList_3(ID_SEGMENT_ENTRY, (u32) sizeof(SegmentEntry), VEX_CHUNKED_BYTES_DEFAULT);
    SegmentIndex_setEntries(index, entries);
    SegmentIndex_setCount(index, 0u);
    if (!entries)
        SegmentIndex_setActive(index, false);
}

u32 SegmentIndex_add(SegmentIndex *index, const char *name, pointer payload) {
    if (!index || !SegmentIndex_active(index))
        return SEGMENT_ENTRY_NONE;
    char folded[SEGMENT_NAME_BYTES];
    if (!VariableSlot_foldName(name, folded))
        return SEGMENT_ENTRY_NONE;
    SegmentEntry *row = (SegmentEntry*) ChunkedList_addSlot(SegmentIndex_entries(index));
    if (!row)
        return SEGMENT_ENTRY_NONE;
    memset(row, 0, sizeof(*row));
    memcpy(SegmentEntry_nameMut(row), folded, strlen(folded) + 1);
    SegmentEntry_setPayload(row, payload);
    u32 entry = ChunkedList_size(SegmentIndex_entries(index)) - 1u;

    u32 position = 0u;
    const char *p = folded;
    while (*p != '\0') {
        const char *dot = strchr(p, '.');
        bytes len = dot ? (bytes)(dot - p) : strlen(p);
        char segment[SEGMENT_NAME_BYTES];
        memcpy(segment, p, len);
        segment[len] = '\0';
        if (!postSegment(index, entry, position, segment)) {
            SegmentEntry_setSegments(row, position);
            SegmentIndex_setCount(index, entry + 1u);
            return SEGMENT_ENTRY_NONE;
        }
        position++;
        if (!dot)
            break;
        p = dot + 1;
    }
    SegmentEntry_setSegments(row, position);
    SegmentIndex_setCount(index, entry + 1u);
    return entry;
}

bytes SegmentIndex_buildFrom(SegmentIndex *index, VariableHashMap *map) {
    if (!index || !map)
        return 0u;
    SegmentIndex_clear(index);
    BuildCtx ctx = { index };
    VariableHashMap_forEach(map, buildFn, &ctx);
    return SegmentIndex_count(index);
}

bytes SegmentIndex_buildFromMini(SegmentIndex *index, VariableMiniMap *map) {
    if (!index || !map)
        return 0u;
    SegmentIndex_clear(index);
    BuildCtx ctx = { index };
    VariableMiniMap_forEach(map, buildFn, &ctx);
    return SegmentIndex_count(index);
}

bytes SegmentIndex_query(const SegmentIndex *index, const char *query, SegmentMatch *out, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!index || !SegmentIndex_active(index) || !query || !out || cap == 0u)
        return 0u;

    char segs[SEGMENT_QUERY_SEGMENTS_MAX][SEGMENT_NAME_BYTES];
    bytes qn = splitFold(query, segs);
    if (qn == 0u)
        return 0u;

    const ChunkedList *lists[SEGMENT_QUERY_SEGMENTS_MAX];
    bytes rarest = 0u;
    for (bytes k = 0u; k < qn; k++) {
        pointer value = VariableHashMap_get(SegmentIndex_postings(index), segs[k]);
        if (value == 0u)
            return 0u;
        lists[k] = (const ChunkedList*) value;
        if (ChunkedList_size(lists[k]) < ChunkedList_size(lists[rarest]))
            rarest = k;
    }

    ;;INTENTION("cold path search is a node walk by design; hot iteration stays DOD")
    const ChunkedList *cand = lists[rarest];
    u32 candCount = ChunkedList_size(cand);
    bytes kept = 0u;
    bytes total = 0u;
    for (u32 i = 0u; i < candCount; i++) {
        const SegmentPosting *p = (const SegmentPosting*) ChunkedList_slot(cand, i);
        if (!p || SegmentPosting_position(p) < rarest)
            continue;
        u32 entry = SegmentPosting_entry(p);
        u32 start = SegmentPosting_position(p) - (u32) rarest;

        bool ok = true;
        for (bytes k = 0u; k < qn; k++) {
            if (!findPosting(lists[k], entry, start + (u32) k)) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;

        const SegmentEntry *e = entryAt(index, entry);
        if (!e)
            continue;
        u32 entrySegments = SegmentEntry_segments(e);
        u32 tier = SEGMENT_MATCH_CONTAINS;
        if (qn == entrySegments && start == 0u)
            tier = SEGMENT_MATCH_EXACT;
        else if (start + (u32) qn == entrySegments)
            tier = SEGMENT_MATCH_SUFFIX;

        SegmentMatch m;
        m.payload = SegmentEntry_payload(e);
        m.entry = entry;
        m.start = start;
        m.tier = tier;
        insertMatch(index, out, cap, &kept, &m);
        total++;
    }

    if (outTruncated)
        *outTruncated = total > kept;
    return total;
}

// GETTERS

u32 SegmentIndex_count(const SegmentIndex *index) {
    return index ? SegmentIndex_countRaw(index) : 0u;
}

bool SegmentIndex_isEmpty(const SegmentIndex *index) {
    if (!index)
        return true;
    return SegmentIndex_count(index) == 0u;
}

// STRING PROJECTIONS (the toString Law)

fn SegmentIndex_toString(const SegmentIndex *index, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!dest || cap == 0u)
        return;
    if (!index) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "SegmentIndex(entries=%u)", SegmentIndex_count(index));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}

fn SegmentIndex_toStringStruct(const SegmentIndex *index, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!dest || cap == 0u)
        return;
    if (!index) {
        snprintf(dest, cap, "nullptr");
        return;
    }
    i32 written = snprintf(dest, cap, "SegmentIndex { active=%s, count=%u, entries=0x%llx, postings=0x%llx }",
                           SegmentIndex_active(index) ? "true" : "false", SegmentIndex_count(index),
                           (u64) (pointer) SegmentIndex_entries(index),
                           (u64) (pointer) SegmentIndex_postings(index));
    if (written < 0 || (bytes) written >= cap) {
        if (outTruncated)
            *outTruncated = true;
    }
}
