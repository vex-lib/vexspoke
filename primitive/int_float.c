#include "primitive/int_f32.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: IntFloat
 * ============================================================================
 * IntFloat primitive (Legacy: primitive/IntFloat.java): a 64-bit payload
 * holding an int32/f32 pair, backed by a fixed 1024-slot BitPool for
 * singletons and the memory arena for arrays. Provides alloc/free,
 * get/set, compare-and-set, type/length introspection, and
 * alloc-with-values convenience. Lives at R2 as a leaf primitive behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Int_f32 (primitive/int_f32.c)
 * ============================================================================
 * IntFloat primitive (Legacy: primitive/IntFloat.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of int_f32 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - IntFloat_init(void)
 *
 * Core Functions:
 *   - IntFloat_shutdown(void)
 *   - IntFloat_alloc(void)
 *   - IntFloat_allocArray(count)
 *   - IntFloat_free(ptr)
 *   - IntFloat_compareAndSet(ptr, expected, value)
 *   - IntFloat_type(ptr)
 *   - IntFloat_length(ptr)
 *   - IntFloat_allocWithValues(v1, v2)
 *
 * Setters:
 *   - IntFloat_set(ptr, value)
 *
 * Getters:
 *   - IntFloat_get(ptr)
 * ============================================================================
 */


BitPool g_int_f32Pool;

bool IntFloat_init(void) {
    g_int_f32Pool = BitPool(8, 1024);
    return BitPool_live(&g_int_f32Pool);
}

void IntFloat_shutdown(void) {
    BitPool_shutdown(&g_int_f32Pool);
}

void *IntFloat_alloc(void) {
    return BitPool_alloc(&g_int_f32Pool, ID_INT_FLOAT);
}

void *IntFloat_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(int64_t);
    // For compound types, elem_size 8 already accounts for stride, but sizeof(int64_t) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 8 != sizeof(int64_t))
        Bytes = count * 8;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_INT_FLOAT), Bytes);
}

void IntFloat_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_int_f32Pool, ptr)) BitPool_free(&g_int_f32Pool, ptr);
    else Memory_free(ptr);
}

int64_t IntFloat_get(void *ptr) {
    if (!ptr)
        return (int64_t) 0;
    return *(int64_t*) ptr;
}

void IntFloat_set(void *ptr, int64_t value) {
    if (!ptr)
        return;
    *(int64_t*) ptr = value;
}

bool IntFloat_compareAndSet(void *ptr, int64_t expected, int64_t value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((int64_t*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 IntFloat_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_int_f32Pool, ptr)) return BitPool_type(&g_int_f32Pool, ptr);
    return Memory_type(ptr);
}

bytes IntFloat_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_int_f32Pool, ptr)) return BitPool_length(&g_int_f32Pool, ptr);
    return Memory_length(ptr);
}

void *IntFloat_allocWithValues(i32 v1, f32 v2) {
    void *ptr = IntFloat_alloc();
    if (!ptr) return nullptr;
    *(i32*) ptr = v1;
    *(f32*) ((u8*) ptr + sizeof(i32)) = v2;
    return ptr;
}
