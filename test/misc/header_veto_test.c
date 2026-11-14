#include "vex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: HeaderVetoTest (header_veto_test/test.c — LOCAL ONLY, uncommitted)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Local proof for the 32-byte frozen header: sugar and hash vetoes fire on corrupted
 * headers, interior and foreign pointers fail closed, free invalidates,
 * class-from-length routing recycles across size classes, and Similar
 * answers identity only. Never committed (repo tests/ policy) — run by
 * hand after allocator surgery.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main()
 * ============================================================================
 */

static int g_failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { printf("[header_veto_test] PASS %s\n", name); } \
    else { printf("[header_veto_test] FAIL %s\n", name); g_failures++; } \
} while (0)

// Exercises memory-header integrity vetoes, invalidation, size classes, and reuse.
int main() {
    printf("=== Running Header Veto Test Suite ===\n");
    MemoryArena *arena = MemoryArena_create(64u << 20);
    CHECK("arena created", arena != nullptr);

    // section 1 Valid blocks read back; Similar answers identity.
    any a = MemoryArena_alloc(arena, TYPE_STRING_ARRAY, 32);
    any b = MemoryArena_alloc(arena, TYPE_STRING_ARRAY, 40);
    any c = MemoryArena_alloc(arena, TYPE_INT_ARRAY, 32);
    CHECK("allocs live", a && b && c);
    CHECK("length reads", Memory_length(a) == 32 && Memory_length(b) == 40);
    CHECK("type reads", Memory_type(a) == TYPE_STRING_ARRAY);
    CHECK("similar same class", Memory_similar(a, b) == true);
    CHECK("similar self", Memory_similar(a, a) == true);
    CHECK("similar diff class", Memory_similar(a, c) == false);
    CHECK("similar null", Memory_similar(nullptr, a) == false && Memory_similar(a, nullptr) == false);

    // section 2 Corrupted redirect vetoes reads (header word 4: pointer).
    u8 *ha = (u8*) a;
    ha[-8] ^= 0xFFu;
    CHECK("corrupt redirect kills length", Memory_length(a) == 0);
    CHECK("corrupt redirect kills type", Memory_type(a) == 0);
    CHECK("corrupt redirect kills similar", Memory_similar(a, b) == false);
    ha[-8] ^= 0xFFu;
    CHECK("restored redirect revives", Memory_length(a) == 32);

    // section 3 Corrupted identity vetoes (header word 1: the id).
    ha[-32] ^= 0xFFu;
    CHECK("corrupt self kills type", Memory_type(a) == 0);
    CHECK("corrupt self kills similar", Memory_similar(a, b) == false);
    ha[-32] ^= 0xFFu;
    CHECK("restored self revives", Memory_type(a) == TYPE_STRING_ARRAY);

    // section 4 Interior and foreign pointers fail closed.
    CHECK("interior pointer fails", Memory_length((u8*) b + 8) == 0);
    CHECK("interior similar fails", Memory_similar((u8*) b + 8, b) == false);
    any foreign = malloc(64);
    pointer fu = (pointer) foreign;
    any alignedForeign = (any) ((fu + 15) & ~15ull);
    CHECK("foreign fails", Memory_length(alignedForeign) == 0 && Memory_type(alignedForeign) == 0);
    CHECK("foreign similar fails", Memory_similar(alignedForeign, b) == false);
    free(foreign);

    // section 5 Free invalidates; d64 free is safe; sweep skips dead.
    MemoryArena_free(arena, c);
    CHECK("freed reads fail", Memory_length(c) == 0 && Memory_type(c) == 0);
    MemoryArena_free(arena, c);
    CHECK("d64 free safe", Memory_length(c) == 0);
    any found[8] = {0};
    bytes n = MemoryArena_findAll(arena, TYPE_STRING_ARRAY, found, 8);
    CHECK("sweep finds live only", n == 2);

    // section 6 Size classes recycle (slab + bump paths).
    bytes sizes[] = { 8, 64, 300, 2000, 9000, 100000 };
    any blocks[6] = {0};
    bool allOk = true;
    for (int i = 0; i < 6; i++) {
        blocks[i] = MemoryArena_alloc(arena, TYPE_BYTE_ARRAY, sizes[i]);
        if (!blocks[i] || Memory_length(blocks[i]) != sizes[i])
            allOk = false;
    }
    CHECK("all classes alloc", allOk);
    for (int i = 0; i < 6; i++)
        MemoryArena_free(arena, blocks[i]);
    bool allDead = true;
    for (int i = 0; i < 6; i++) {
        if (Memory_length(blocks[i]) != 0)
            allDead = false;
    }
    CHECK("all classes free+invalidate", allDead);
    any re = MemoryArena_alloc(arena, TYPE_BYTE_ARRAY, 300);
    CHECK("reuse after free", re != nullptr && Memory_length(re) == 300);

    // section 7 Realloc preserves identity across the move.
    any r = MemoryArena_alloc(arena, TYPE_INT_ARRAY, 16);
    memset(r, 0xAB, 16);
    any r2 = MemoryArena_realloc(arena, r, 64);
    CHECK("realloc valid", r2 != nullptr && Memory_type(r2) == TYPE_INT_ARRAY && Memory_length(r2) == 64);
    CHECK("realloc moved old dead", r2 == r || Memory_length(r) == 0);

    MemoryArena_destroy(arena);
    printf("\n=== Header Veto Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}
