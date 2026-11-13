#include "vex.h"

#include "primitive/fixed32.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Fixed32
 * ============================================================================
 * Fixed32 primitive, ported from primitive/Fixed32.java. A 4-byte int32
 * payload served from a 1024-slot BitPool for singletons and from the memory
 * arena for arrays; Fixed32_free routes to whichever pool owns the pointer.
 * compareAndSet is a lock-free __atomic_compare_exchange on the payload, so
 * the primitive f64s as a small atomic counter. Getters return safe
 * defaults on null.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Fixed32 (primitive/fixed32.c)
 * ============================================================================
 * Fixed32 primitive (Legacy: primitive/Fixed32.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of fixed32 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Fixed32_init(void)
 *
 * Core Functions:
 *   - Fixed32_shutdown(void)
 *   - Fixed32_alloc(void)
 *   - Fixed32_allocArray(count)
 *   - Fixed32_free(ptr)
 *   - Fixed32_compareAndSet(ptr, expected, value)
 *   - Fixed32_type(ptr)
 *   - Fixed32_length(ptr)
 *   - Fixed32_allocWithValue(value)
 *
 * Setters:
 *   - Fixed32_set(ptr, value)
 *
 * Getters:
 *   - Fixed32_get(ptr)
 * ============================================================================
 */


BitPool g_fixed32Pool;

bool Fixed32_init(void) {
    g_fixed32Pool = BitPool(4, 1024);
    return BitPool_live(&g_fixed32Pool);
}

void Fixed32_shutdown(void) {
    BitPool_shutdown(&g_fixed32Pool);
}

any Fixed32_alloc(void) {
    return BitPool_alloc(&g_fixed32Pool, ID_FIXED32);
}

any Fixed32_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(i32);
    // For compound types, elem_size 4 already accounts for stride, but sizeof(i32) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 4 != sizeof(i32))
        Bytes = count * 4;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_FIXED32), Bytes);
}

void Fixed32_free(any ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_fixed32Pool, ptr)) BitPool_free(&g_fixed32Pool, ptr);
    else Memory_free(ptr);
}

i32 Fixed32_get(any ptr) {
    if (!ptr)
        return (i32) 0;
    return *(i32*) ptr;
}

void Fixed32_set(any ptr, i32 value) {
    if (!ptr)
        return;
    *(i32*) ptr = value;
}

bool Fixed32_compareAndSet(any ptr, i32 expected, i32 value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((i32*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Fixed32_type(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_fixed32Pool, ptr)) return BitPool_type(&g_fixed32Pool, ptr);
    return Memory_type(ptr);
}

bytes Fixed32_length(any ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_fixed32Pool, ptr)) return BitPool_length(&g_fixed32Pool, ptr);
    return Memory_length(ptr);
}

any Fixed32_allocWithValue(i32 value) {
    any ptr = Fixed32_alloc();
    if (ptr) Fixed32_set(ptr, value);
    return ptr;
}
