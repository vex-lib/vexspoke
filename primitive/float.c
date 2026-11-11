#include "primitive/vex-f32.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Float
 * ============================================================================
 * Float primitive (Legacy: primitive/Float.java): a procedural class over a
 * process-wide BitPool (4-byte elements, 1024 fixed slots) with a memory
 * arena fallback for large arrays. Float_alloc hands out pool slots typed
 * ID_FLOAT; Float_allocArray switches to the arena when contiguity or size
 * demands it, and Float_free routes to whichever owner holds the pointer.
 * Float_compareAndSet compares the payload's IEEE-754 bit pattern as an
 * int32 so the CAS is exact. Float_init/Float_shutdown bracket the pool.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Float (primitive/f32.c)
 * ============================================================================
 * Float primitive (Legacy: primitive/Float.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of f32 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Float_init(void)
 *
 * Core Functions:
 *   - Float_shutdown(void)
 *   - Float_alloc(void)
 *   - Float_allocArray(count)
 *   - Float_free(ptr)
 *   - Float_compareAndSet(ptr, expected, value)
 *   - Float_type(ptr)
 *   - Float_length(ptr)
 *   - Float_allocWithValue(value)
 *
 * Setters:
 *   - Float_set(ptr, value)
 *
 * Getters:
 *   - Float_get(ptr)
 * ============================================================================
 */


BitPool g_f32Pool;

bool Float_init(void) {
    g_f32Pool = BitPool(4, 1024);
    return BitPool_live(&g_f32Pool);
}

void Float_shutdown(void) {
    BitPool_shutdown(&g_f32Pool);
}

void *Float_alloc(void) {
    return BitPool_alloc(&g_f32Pool, ID_FLOAT);
}

void *Float_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(f32);
    // For compound types, elem_size 4 already accounts for stride, but sizeof(f32) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 4 != sizeof(f32))
        Bytes = count * 4;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_FLOAT), Bytes);
}

void Float_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_f32Pool, ptr)) BitPool_free(&g_f32Pool, ptr);
    else Memory_free(ptr);
}

f32 Float_get(void *ptr) {
    if (!ptr)
        return (f32) 0;
    return *(f32*) ptr;
}

void Float_set(void *ptr, f32 value) {
    if (!ptr)
        return;
    *(f32*) ptr = value;
}

bool Float_compareAndSet(void *ptr, f32 expected, f32 value) {
    if (!ptr)
        return false;
    i32 expBits;
    i32 valBits;
    memcpy(&expBits, &expected, sizeof(expBits));
    memcpy(&valBits, &value, sizeof(valBits));
    return __atomic_compare_exchange_n((i32*) ptr, &expBits, valBits, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Float_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_f32Pool, ptr)) return BitPool_type(&g_f32Pool, ptr);
    return Memory_type(ptr);
}

bytes Float_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_f32Pool, ptr)) return BitPool_length(&g_f32Pool, ptr);
    return Memory_length(ptr);
}

void *Float_allocWithValue(f32 value) {
    void *ptr = Float_alloc();
    if (ptr) Float_set(ptr, value);
    return ptr;
}
