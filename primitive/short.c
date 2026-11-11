#include "primitive/short.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Short
 * ============================================================================
 * Short primitive (Legacy: primitive/Short.java). Singleton 2-byte payloads
 * come from a fixed 1024-slot BitPool; arrays fall back to the memory arena
 * because the pool is not sized for large contiguous blocks. Short_free routes
 * to the owning allocator via BitPool_contains, and Short_compareAndSet
 * provides an atomic CAS on the payload.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Short (primitive/short.c)
 * ============================================================================
 * Short primitive (Legacy: primitive/Short.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of short payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Short_init(void)
 *
 * Core Functions:
 *   - Short_shutdown(void)
 *   - Short_alloc(void)
 *   - Short_allocArray(count)
 *   - Short_free(ptr)
 *   - Short_compareAndSet(ptr, expected, value)
 *   - Short_type(ptr)
 *   - Short_length(ptr)
 *   - Short_allocWithValue(value)
 *
 * Setters:
 *   - Short_set(ptr, value)
 *
 * Getters:
 *   - Short_get(ptr)
 * ============================================================================
 */


BitPool g_shortPool;

bool Short_init(void) {
    g_shortPool = BitPool(2, 1024);
    return BitPool_live(&g_shortPool);
}

void Short_shutdown(void) {
    BitPool_shutdown(&g_shortPool);
}

void *Short_alloc(void) {
    return BitPool_alloc(&g_shortPool, ID_SHORT);
}

void *Short_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof();
    // For compound types, elem_size 2 already accounts for stride, but sizeof() is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 2 != sizeof())
        Bytes = count * 2;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_SHORT), Bytes);
}

void Short_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_shortPool, ptr)) BitPool_free(&g_shortPool, ptr);
    else Memory_free(ptr);
}

 Short_get(void *ptr) {
    if (!ptr)
        return () 0;
    return *(*) ptr;
}

void Short_set(void *ptr,  value) {
    if (!ptr)
        return;
    *(*) ptr = value;
}

bool Short_compareAndSet(void *ptr,  expected,  value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Short_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_shortPool, ptr)) return BitPool_type(&g_shortPool, ptr);
    return Memory_type(ptr);
}

bytes Short_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_shortPool, ptr)) return BitPool_length(&g_shortPool, ptr);
    return Memory_length(ptr);
}

void *Short_allocWithValue( value) {
    void *ptr = Short_alloc();
    if (ptr) Short_set(ptr, value);
    return ptr;
}
