#include "primitive/int_f64.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Intf64
 * ============================================================================
 * Intf64 primitive (Legacy: primitive/Intf64.java). 16-byte payload
 * (int32 + d64) singletons come from a fixed 1024-slot BitPool; arrays fall
 * back to the memory arena because the pool is not sized for large contiguous
 * blocks. Intf64_free routes to the owning allocator via BitPool_contains,
 * and Intf64_compareAndSet provides an atomic CAS on the payload.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Int_f64 (primitive/int_f64.c)
 * ============================================================================
 * Intf64 primitive (Legacy: primitive/Intf64.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of int_f64 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Intf64_init(void)
 *
 * Core Functions:
 *   - Intf64_shutdown(void)
 *   - Intf64_alloc(void)
 *   - Intf64_allocArray(count)
 *   - Intf64_free(ptr)
 *   - Intf64_compareAndSet(ptr, expected, value)
 *   - Intf64_type(ptr)
 *   - Intf64_length(ptr)
 *   - Intf64_allocWithValues(v1, v2)
 *
 * Setters:
 *   - Intf64_set(ptr, value)
 *
 * Getters:
 *   - Intf64_get(ptr)
 * ============================================================================
 */


BitPool g_int_f64Pool;

bool Intf64_init(void) {
    g_int_f64Pool = BitPool(16, 1024);
    return BitPool_live(&g_int_f64Pool);
}

void Intf64_shutdown(void) {
    BitPool_shutdown(&g_int_f64Pool);
}

void *Intf64_alloc(void) {
    return BitPool_alloc(&g_int_f64Pool, ID_INT_f64);
}

void *Intf64_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(int64_t);
    // For compound types, elem_size 16 already accounts for stride, but sizeof(int64_t) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 16 != sizeof(int64_t))
        Bytes = count * 16;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_INT_f64), Bytes);
}

void Intf64_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_int_f64Pool, ptr)) BitPool_free(&g_int_f64Pool, ptr);
    else Memory_free(ptr);
}

int64_t Intf64_get(void *ptr) {
    if (!ptr)
        return (int64_t) 0;
    return *(int64_t*) ptr;
}

void Intf64_set(void *ptr, int64_t value) {
    if (!ptr)
        return;
    *(int64_t*) ptr = value;
}

bool Intf64_compareAndSet(void *ptr, int64_t expected, int64_t value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((int64_t*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Intf64_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_int_f64Pool, ptr)) return BitPool_type(&g_int_f64Pool, ptr);
    return Memory_type(ptr);
}

bytes Intf64_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_int_f64Pool, ptr)) return BitPool_length(&g_int_f64Pool, ptr);
    return Memory_length(ptr);
}

void *Intf64_allocWithValues(i32 v1, d64 v2) {
    void *ptr = Intf64_alloc();
    if (!ptr) return nullptr;
    *(i32*) ptr = v1;
    *(d64*) ((u8*) ptr + sizeof(i32)) = v2;
    return ptr;
}
