#include "vex.h"
#include "primitive/long.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Long
 * ============================================================================
 * Long primitive, ported from primitive/Long.java. An 8-byte int64 payload
 * served from a 1024-slot BitPool for singletons and from the memory arena
 * for arrays; Long_free routes to whichever pool owns the pointer.
 * compareAndSet is a lock-free __atomic_compare_exchange on the payload, so
 * the primitive f64s as a small atomic counter. Getters return safe
 * defaults on null.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Long (primitive/i64.c)
 * ============================================================================
 * Long primitive (Legacy: primitive/Long.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of i64 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Long_init()
 *
 * Core Functions:
 *   - Long_shutdown()
 *   - Long_alloc()
 *   - Long_allocArray(count)
 *   - Long_free(ptr)
 *   - Long_compareAndSet(ptr, expected, value)
 *   - Long_type(ptr)
 *   - Long_length(ptr)
 *   - Long_allocWithValue(value)
 *
 * Setters:
 *   - Long_set(ptr, value)
 *
 * Getters:
 *   - Long_get(ptr)
 * ============================================================================
 */


BitPool g_longPool;

bool Long_init() {
    g_longPool = BitPool(8, 1024);
    return BitPool_live(&g_longPool);
}

fn Long_shutdown() {
    BitPool_shutdown(&g_longPool);
}

any Long_alloc() {
    return BitPool_alloc(&g_longPool, ID_LONG);
}

any Long_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(i64);
    // For compound types, elem_size 8 already accounts for stride, but sizeof(i64) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 8 != sizeof(i64))
        Bytes = count * 8;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_LONG), Bytes);
}

fn Long_free(any ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_longPool, ptr)) BitPool_free(&g_longPool, ptr);
    else Memory_free(ptr);
}

i64 Long_get(any ptr) {
    if (!ptr)
        return (i64) 0;
    return *(i64*) ptr;
}

fn Long_set(any ptr, i64 value) {
    if (!ptr)
        return;
    *(i64*) ptr = value;
}

bool Long_compareAndSet(any ptr, i64 expected, i64 value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((i64*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Long_type(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_longPool, ptr)) return BitPool_type(&g_longPool, ptr);
    return Memory_type(ptr);
}

bytes Long_length(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_longPool, ptr)) return BitPool_length(&g_longPool, ptr);
    return Memory_length(ptr);
}

any Long_allocWithValue(i64 value) {
    any ptr = Long_alloc();
    if (ptr) Long_set(ptr, value);
    return ptr;
}
