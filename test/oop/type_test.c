// vexspoke/test/oop/type_test.c — the Type owner test (Per-File Battle Test Law).
//
// Type is the id algebra every block header rides on, so the battle rows are
// the VALUE BOUNDARY MATRIX of the bit fields (each field is isolated), the
// registry seam's ADVERSARIAL rejections (zero/foreign project Bytes, nullptr
// with a non-zero count, PROJ_VEXSPOKE itself), IDEMPOTENCE, exponential slate
// growth under many projects, and the FAILURE ATOMICITY of every rejection.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "oop/vexspoke_type.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// A project byte with no stray bits: the field occupies bits 52..59.
static u64 projByte(u64 byte) {
    return (byte << 52) & MASK_PROJECT;
}

// Checks type bit fields, predicates, parent registration, and project isolation.
int main(void) {
    // --- Bit field isolation: make then read each field back exactly.
    u64 id = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_INT);
    CHECK(id == TYPE_INT_SINGLETON);
    CHECK(Type_class(id) == ID_INT);
    CHECK(Type_form(id) == FORM_SINGLETON);
    CHECK(Type_project(id) == PROJ_VEXSPOKE);
    CHECK(Type_class(0xFFFFFFFFull) == 0x00FFFFFFu);         // class keeps 24 bits
    CHECK((SUGAR_VEX & MASK_SUGAR) == SUGAR_VEX);          // reserved "векс" slot
    CHECK(Type_form(FORM_STRUCT_ARRAY) == FORM_STRUCT_ARRAY);

    // --- Form predicates: one true, the disjoint neighbours false.
    CHECK(Type_isSingleton(TYPE_INT_SINGLETON));
    CHECK(!Type_isArray(TYPE_INT_SINGLETON));
    CHECK(Type_isArray(TYPE_INT_ARRAY));
    CHECK(Type_isPointer(TYPE_INT_POINTER));
    CHECK(Type_isStruct(FORM_STRUCT_SINGLETON));
    CHECK(!Type_isStruct(0));
    CHECK(!Type_isStruct(FORM_SINGLETON));
    CHECK(Type_isStructSingleton(SPOKE_TYPE_VARIABLE_SLOT));
    CHECK(Type_isStructArray(FORM_STRUCT_ARRAY));
    CHECK(Type_isStructSOA(FORM_ARRAY_SOA));
    CHECK(Type_isStructAOS(FORM_ARRAY_AOS));
    CHECK(Type_isStructCoexistent(FORM_STRUCT_COEXISTENT));
    CHECK(Type_isStructPointer(FORM_STRUCT_POINTER));
    CHECK(Type_isPrimitive(TYPE_INT_SINGLETON));
    CHECK(Type_isPrimitive(TYPE_INT_ARRAY));
    CHECK(!Type_isPrimitive(FORM_STRUCT_SINGLETON));

    // --- Modifier / wrapper predicates.
    CHECK(Type_isGlobal(TYPE_GLOBAL));
    CHECK(Type_isLocale(TYPE_LOCAL));
    CHECK(Type_isTransient(PROJ_VEXSPOKE | FORM_SINGLETON | MOD_TRANSIENT | ID_INT));
    CHECK(Type_isReactive(TYPE_REACTIVE));
    CHECK(Type_isProactive(TYPE_PASSIVE));
    CHECK(Type_isProbable(TYPE_PROBABLE));
    CHECK(Type_isProbableObjects(TYPE_PROBABLE_OBJECTS));
    CHECK(Type_isFuture(TYPE_FUTURE));
    CHECK(Type_isChoice(TYPE_CHOICE));
    CHECK(!Type_isReactive(TYPE_PASSIVE));

    // --- Architecture byte: each project reports itself; bare ids are vexspoke.
    CHECK(Type_arch(TYPE_INT_SINGLETON) == ARCH_VEXSPOKE);
    CHECK(Type_arch(ID_INT) == ARCH_VEXSPOKE);               // bare id → vexspoke
    CHECK(Type_arch(projByte(2) | ID_INT) == ARCH_GRAPHVEX);
    CHECK(Type_arch(projByte(3) | ID_INT) == ARCH_HOTCWAP);
    CHECK(Type_arch(projByte(4) | ID_INT) == ARCH_DARLING);
    CHECK(Type_arch(projByte(5) | ID_INT) == ARCH_APIHAVEN);
    CHECK(Type_arch(projByte(6) | ID_INT) == ARCH_DARKBASE);
    CHECK(Type_isDarkbase(projByte(6) | ID_INT));
    CHECK(Type_isVexspoke(TYPE_INT_SINGLETON));
    CHECK(Type_isDarling(projByte(4) | ID_INT));

    // --- vexspoke's own legacy chain: buffer family rolls up to ID_BUFFER. The
    // resolver is now Relational Engine's; the owner grants the bare chain once.
    static const u32 bareChain[0x64] = { [0x50 ... 0x63] = ID_BUFFER };
    CHECK(Type_registerBareParents(bareChain, 0x64u));
    CHECK(Type_getParentClass(ID_ACCUMULUATION_BUFFER) == ID_BUFFER);
    CHECK(Type_getParentClass(ID_VISIBILITY_BUFFER) == ID_BUFFER);
    CHECK(Type_getParentClass(ID_BUFFER) == ID_BUFFER);      // the root
    CHECK(Type_getParentClass(ID_INT) == ID_INT);            // bare root

    // --- Registry seam: ADVERSARIAL rejections are atomic (false, no state).
    static const u32 chain[4] = { 0, 0, 1, 2 };
    CHECK(!Type_registerParents(0u, chain, 4));                         // zero project
    CHECK(!Type_registerParents(0x1ull, chain, 4));                     // stray bits
    CHECK(!Type_registerParents(PROJ_VEXSPOKE, chain, 4));              // vexspoke itself
    CHECK(!Type_registerParents(projByte(0x20), nullptr, 3));           // null table, count!=0
    CHECK(Type_registerParents(projByte(0x20), nullptr, 0));            // null table, count==0 ok
    CHECK(Type_getParentClass(projByte(0x20) | 9) == 9);                // unlisted → root

    // --- A real chain: 3 → 2 → 1 (root).
    CHECK(Type_registerParents(projByte(0x21), chain, 4));
    CHECK(Type_getParentClass(projByte(0x21) | 3) == 2u);
    CHECK(Type_getParentClass(projByte(0x21) | 2) == 1u);
    CHECK(Type_getParentClass(projByte(0x21) | 1) == 1u);               // 0 row = root
    CHECK(Type_getParentClass(projByte(0x21) | 0) == 0u);              // class 0 → root
    CHECK(Type_getParentClass(projByte(0x21) | 99) == 99u);            // past count → root
    CHECK(Type_isA(projByte(0x21) | 3, projByte(0x21) | 1));
    CHECK(Type_isA(projByte(0x21) | 1, projByte(0x21) | 1));            // self
    CHECK(!Type_isA(projByte(0x21) | 1, projByte(0x21) | 3));           // ancestor is not descendant

    // --- Idempotence: re-registering a project replaces its table.
    static const u32 chain2[4] = { 0, 0, 0, 1 };
    CHECK(Type_registerParents(projByte(0x21), chain2, 4));
    CHECK(Type_getParentClass(projByte(0x21) | 3) == 1u);               // new table won

    // --- Cyclic parent tables are rejected; prior state is preserved.
    static const u32 cyclic[4] = { 0, 2, 1, 2 };  // 1->2->1 never roots
    CHECK(!Type_registerParents(projByte(0x22), cyclic, 4));
    CHECK(Type_getParentClass(projByte(0x22) | 1) == 1u);              // nothing stored
    CHECK(!Type_isA(projByte(0x22) | 1, projByte(0x22) | 2));

    // A self-parent is a root, not a cycle (0 = the class is its own parent).
    static const u32 selfRoot[3] = { 0, 1, 0 };
    CHECK(Type_registerParents(projByte(0x23), selfRoot, 3));
    CHECK(Type_getParentClass(projByte(0x23) | 1) == 1u);
    CHECK(Type_isA(projByte(0x23) | 1, projByte(0x23) | 1));
    CHECK(!Type_isA(projByte(0x23) | 1, projByte(0x23) | 2));          // 2 is a root, not a child

    // --- A long legal chain resolves end to end (walk bounded by the table,
    // never capped below it).
    static u32 longChain[300];
    longChain[0] = 0u;
    for (u32 i = 1u; i < 300u; i++)
        longChain[i] = i - 1u;
    CHECK(Type_registerParents(projByte(0x24), longChain, 300));
    CHECK(Type_isA(projByte(0x24) | 299, projByte(0x24) | 1));
    CHECK(Type_isA(projByte(0x24) | 299, projByte(0x24) | 299));       // self
    CHECK(!Type_isA(projByte(0x24) | 1, projByte(0x24) | 299));        // not a descendant

    // --- Cross-project class-number collision: chains never bleed across
    // projects; the ancestor target stays project-invariant (masked to class).
    static const u32 chainA[4] = { 0, 0, 1, 2 };  // A: 3->2->1
    static const u32 chainB[4] = { 0, 0, 0, 1 };  // B: 3->1, 2 root
    CHECK(Type_registerParents(projByte(0x40), chainA, 4));
    CHECK(Type_registerParents(projByte(0x41), chainB, 4));
    CHECK(Type_isA(projByte(0x40) | 3, projByte(0x40) | 1));
    CHECK(Type_isA(projByte(0x40) | 3, projByte(0x40) | 2));
    CHECK(Type_isA(projByte(0x41) | 3, projByte(0x41) | 1));
    CHECK(!Type_isA(projByte(0x41) | 3, projByte(0x41) | 2));          // B: 2 is root
    CHECK(!Type_isA(projByte(0x40) | 2, projByte(0x40) | 3));          // direction
    CHECK(Type_isA(projByte(0x40) | 3, projByte(0x41) | 1));           // target resolves in A's project

    // --- Exponential slate growth: many projects must all resolve.
    static const u32 flat[4] = { 0, 0, 0, 0 };
    for (u64 b = 0x30; b < 0x30 + 24; b++)
        CHECK(Type_registerParents(projByte(b), flat, 4));
    for (u64 b = 0x30; b < 0x30 + 24; b++) {
        CHECK(Type_getParentClass(projByte(b) | 1) == 1u);              // flat → root
        CHECK(Type_getParentClass(projByte(b) | 2) == 2u);
    }

    if (g_failures == 0) {
        printf("type_test: all assertions held\n");
        return 0;
    }
    printf("type_test: %d FAILURES\n", g_failures);
    return 1;
}
