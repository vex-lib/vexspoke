#include "vex.h"
#include "c23/equals.h"

#include "nio/mem.h"
#include "substrate/relational/symbol_table.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Equals
 * ============================================================================
 * Relational equality: the C answer to Java `.equals()`. Identity first, then
 * the block identity header (type plus length must match), then the payload
 * Bytes compared directly. Foreign pointers prove identity only. The header
 * check keeps the cold path crash-free on mismatched or foreign blocks that
 * happen to collide on content.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Equals (c23/equals.c)
 * ============================================================================
 * relational equality: identity, then block header, then payload Bytes.
 *
 * STRUCT FIELDS: none — procedural (operates on generic memory blocks)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - isEqual(a, b.sh)                    : Identity + header + payload equality
 *   - isEquallyNamed(v, nameA, nameB)  : Registry form via SymbolTable symbols
 * ============================================================================
 */

// Compares identity first, then matching block type, length, and payload bytes.
bool isEqual(const_any a, const_any b) {
    if (a == b)
        return true;
    if (!a || !b)
        return false;

    const u64 ta = Memory_type((any) a);
    const u64 tb = Memory_type((any) b);

    if (ta == 0 || tb == 0)
        return false;
    if (ta != tb)
        return false;

    const bytes la = Memory_length((any) a);
    const bytes lb = Memory_length((any) b);

    if (la != lb)
        return false;

    const auto pa = (const u8*) a;
    const auto pb = (const u8*) b;
    return memcmp(pa, pb, la) == 0;
}

// Resolves both names in the table and compares their referenced values.
bool isEquallyNamed(SymbolTable *v, const char *nameA, const char *nameB) {
    if (!v || !nameA || !nameB)
        return false;
    i32 idA = SymbolTable_getId(v, nameA);
    i32 idB = SymbolTable_getId(v, nameB);
    if (idA < 0 || idB < 0)
        return false;
    if (SymbolTable_getClassId(v, idA) != SymbolTable_getClassId(v, idB))
        return false;
    const_any pa = (const_any) SymbolTable_getPointer(v, idA);
    const_any pb = (const_any) SymbolTable_getPointer(v, idB);
    return isEqual(pa, pb);
}
