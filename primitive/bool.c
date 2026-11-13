#include "vex.h"

#include "primitive/bool.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Bool
 * ============================================================================
 * Bool primitive (Legacy: primitive/Bool.java): a procedural class over a
 * process-wide BitPool (1-byte elements, 1024 fixed slots) with a memory
 * arena fallback for large arrays. Bool_alloc hands out pool slots typed
 * ID_BOOL; Bool_allocArray switches to the arena when contiguity or size
 * demands it, and Bool_free routes to whichever owner holds the pointer.
 * Bool_compareAndSet provides an atomic compare-exchange on the payload.
 * Bool_init/Bool_shutdown bracket the pool's lifetime.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Bool (primitive/bool.c)
 * ============================================================================
 * Bool primitive (Legacy: primitive/Bool.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of bool payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Bool_init(void)
 *
 * Core Functions:
 *   - Bool_shutdown(void)
 *   - Bool_alloc(void)
 *   - Bool_allocArray(count)
 *   - Bool_free(ptr)
 *   - Bool_compareAndSet(ptr, expected, value)
 *   - Bool_type(ptr)
 *   - Bool_length(ptr)
 *   - Bool_allocWithValue(value)
 *
 * Setters:
 *   - Bool_set(ptr, value)
 *
 * Getters:
 *   - Bool_get(ptr)
 * ============================================================================
 */


BitPool g_boolPool;

bool Bool_init(void) {
    g_boolPool = BitPool(1, 1024);
    return BitPool_live(&g_boolPool);
}

void Bool_shutdown(void) {
    BitPool_shutdown(&g_boolPool);
}

any Bool_alloc(void) {
    return BitPool_alloc(&g_boolPool, ID_BOOL);
}

any Bool_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(bool);
    // For compound types, elem_size 1 already accounts for stride, but sizeof(bool) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 1 != sizeof(bool))
        Bytes = count * 1;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_BOOL), Bytes);
}

void Bool_free(any ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_boolPool, ptr)) BitPool_free(&g_boolPool, ptr);
    else Memory_free(ptr);
}

bool Bool_get(any ptr) {
    if (!ptr)
        return (bool) 0;
    return *(bool*) ptr;
}

void Bool_set(any ptr, bool value) {
    if (!ptr)
        return;
    *(bool*) ptr = value;
}

bool Bool_compareAndSet(any ptr, bool expected, bool value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((bool*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Bool_type(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_boolPool, ptr)) return BitPool_type(&g_boolPool, ptr);
    return Memory_type(ptr);
}

bytes Bool_length(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_boolPool, ptr)) return BitPool_length(&g_boolPool, ptr);
    return Memory_length(ptr);
}

any Bool_allocWithValue(bool value) {
    any ptr = Bool_alloc();
    if (ptr) Bool_set(ptr, value);
    return ptr;
}
