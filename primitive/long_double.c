#include "primitive/long_f64.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Long_f64
 * ============================================================================
 * Longf64 primitive over a global 16-byte-slot BitPool (1024 slots) with
 * memory-arena fallback for arrays: alloc/free route through BitPool when the
 * pointer is pool-owned, otherwise through memory. The payload is an int64
 * plus a d64 in the second 8-byte half (allocWithValues writes both);
 * get/set address the int64 half, and compareAndSet is a seq-cst
 * __atomic_compare_exchange_n. type/length dispatch through the owning
 * allocator so callers never pierce the pool/arena split.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Long_f64 (primitive/long_f64.c)
 * ============================================================================
 * Longf64 primitive (Legacy: primitive/Longf64.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of long_f64 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Longf64_init(void)
 *
 * Core Functions:
 *   - Longf64_shutdown(void)
 *   - Longf64_alloc(void)
 *   - Longf64_allocArray(count)
 *   - Longf64_free(ptr)
 *   - Longf64_compareAndSet(ptr, expected, value)
 *   - Longf64_type(ptr)
 *   - Longf64_length(ptr)
 *   - Longf64_allocWithValues(v1, v2)
 *
 * Setters:
 *   - Longf64_set(ptr, value)
 *
 * Getters:
 *   - Longf64_get(ptr)
 * ============================================================================
 */


BitPool g_long_f64Pool;

bool Longf64_init(void) {
    g_long_f64Pool = BitPool(16, 1024);
    return BitPool_live(&g_long_f64Pool);
}

void Longf64_shutdown(void) {
    BitPool_shutdown(&g_long_f64Pool);
}

void *Longf64_alloc(void) {
    return BitPool_alloc(&g_long_f64Pool, ID_LONG_f64);
}

void *Longf64_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(int64_t);
    // For compound types, elem_size 16 already accounts for stride, but sizeof(int64_t) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 16 != sizeof(int64_t))
        Bytes = count * 16;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_LONG_f64), Bytes);
}

void Longf64_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_long_f64Pool, ptr)) BitPool_free(&g_long_f64Pool, ptr);
    else Memory_free(ptr);
}

int64_t Longf64_get(void *ptr) {
    if (!ptr)
        return (int64_t) 0;
    return *(int64_t*) ptr;
}

void Longf64_set(void *ptr, int64_t value) {
    if (!ptr)
        return;
    *(int64_t*) ptr = value;
}

bool Longf64_compareAndSet(void *ptr, int64_t expected, int64_t value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((int64_t*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Longf64_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_long_f64Pool, ptr)) return BitPool_type(&g_long_f64Pool, ptr);
    return Memory_type(ptr);
}

bytes Longf64_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_long_f64Pool, ptr)) return BitPool_length(&g_long_f64Pool, ptr);
    return Memory_length(ptr);
}

void *Longf64_allocWithValues(int64_t v1, d64 v2) {
    void *ptr = Longf64_alloc();
    if (!ptr) return nullptr;
    *(int64_t*) ptr = v1;
    *(d64*) ((u8*) ptr + sizeof(int64_t)) = v2;
    return ptr;
}
