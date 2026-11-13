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
 * MODULE: Long (primitive/long.c)
 * ============================================================================
 * Long primitive (Legacy: primitive/Long.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of long payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Long_init(void)
 *
 * Core Functions:
 *   - Long_shutdown(void)
 *   - Long_alloc(void)
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

bool Long_init(void) {
    g_longPool = BitPool(8, 1024);
    return BitPool_live(&g_longPool);
}

void Long_shutdown(void) {
    BitPool_shutdown(&g_longPool);
}

any Long_alloc(void) {
    return BitPool_alloc(&g_longPool, ID_LONG);
}

any Long_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(int64_t);
    // For compound types, elem_size 8 already accounts for stride, but sizeof(int64_t) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 8 != sizeof(int64_t))
        Bytes = count * 8;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_LONG), Bytes);
}

void Long_free(any ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_longPool, ptr)) BitPool_free(&g_longPool, ptr);
    else Memory_free(ptr);
}

int64_t Long_get(any ptr) {
    if (!ptr)
        return (int64_t) 0;
    return *(int64_t*) ptr;
}

void Long_set(any ptr, int64_t value) {
    if (!ptr)
        return;
    *(int64_t*) ptr = value;
}

bool Long_compareAndSet(any ptr, int64_t expected, int64_t value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((int64_t*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
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

any Long_allocWithValue(int64_t value) {
    any ptr = Long_alloc();
    if (ptr) Long_set(ptr, value);
    return ptr;
}
