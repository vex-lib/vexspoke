#include "primitive/int.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Int
 * ============================================================================
 * Int primitive (Legacy: primitive/Int.java): 32-bit int payloads backed by a
 * fixed 1024-slot BitPool for singletons and the memory arena for arrays.
 * Provides alloc/free, get/set, compare-and-set, type/length introspection,
 * and alloc-with-value convenience. Lives at R2 as a leaf primitive behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Int (primitive/int.c)
 * ============================================================================
 * Int primitive (Legacy: primitive/Int.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of int payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Int_init(void)
 *
 * Core Functions:
 *   - Int_shutdown(void)
 *   - Int_alloc(void)
 *   - Int_allocArray(count)
 *   - Int_free(ptr)
 *   - Int_compareAndSet(ptr, expected, value)
 *   - Int_type(ptr)
 *   - Int_length(ptr)
 *   - Int_allocWithValue(value)
 *
 * Setters:
 *   - Int_set(ptr, value)
 *
 * Getters:
 *   - Int_get(ptr)
 * ============================================================================
 */


BitPool g_intPool;

bool Int_init(void) {
    g_intPool = BitPool(4, 1024);
    return BitPool_live(&g_intPool);
}

void Int_shutdown(void) {
    BitPool_shutdown(&g_intPool);
}

void *Int_alloc(void) {
    return BitPool_alloc(&g_intPool, ID_INT);
}

void *Int_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(i32);
    // For compound types, elem_size 4 already accounts for stride, but sizeof(i32) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 4 != sizeof(i32))
        Bytes = count * 4;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_INT), Bytes);
}

void Int_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_intPool, ptr)) BitPool_free(&g_intPool, ptr);
    else Memory_free(ptr);
}

i32 Int_get(void *ptr) {
    if (!ptr)
        return (i32) 0;
    return *(i32*) ptr;
}

void Int_set(void *ptr, i32 value) {
    if (!ptr)
        return;
    *(i32*) ptr = value;
}

bool Int_compareAndSet(void *ptr, i32 expected, i32 value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((i32*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Int_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_intPool, ptr)) return BitPool_type(&g_intPool, ptr);
    return Memory_type(ptr);
}

bytes Int_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_intPool, ptr)) return BitPool_length(&g_intPool, ptr);
    return Memory_length(ptr);
}

void *Int_allocWithValue(i32 value) {
    void *ptr = Int_alloc();
    if (ptr) Int_set(ptr, value);
    return ptr;
}
