#include "primitive/d64.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: d64
 * ============================================================================
 * d64 primitive (Legacy: primitive/d64.java): a 1024-slot BitPool of
 * 8-byte payloads plus arena-backed arrays for larger allocations. Exists
 * because the relational type system needs typed d64 blocks with
 * lock-free alloc/free and CAS. memory: single blocks come from the fixed
 * BitPool; arrays go through the memory arena with FORM_ARRAY type ids.
 * Lifetime: the f64_init/f64_shutdown pair; f64_free routes to
 * BitPool or memory by containment.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: d64 (primitive/d64.c)
 * ============================================================================
 * d64 primitive (Legacy: primitive/d64.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of d64 payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - f64_init(void)
 *
 * Core Functions:
 *   - f64_shutdown(void)
 *   - f64_alloc(void)
 *   - f64_allocArray(count)
 *   - f64_free(ptr)
 *   - f64_compareAndSet(ptr, expected, value)
 *   - f64_type(ptr)
 *   - f64_length(ptr)
 *   - f64_allocWithValue(value)
 *
 * Setters:
 *   - f64_set(ptr, value)
 *
 * Getters:
 *   - f64_get(ptr)
 * ============================================================================
 */


BitPool g_f64Pool;

bool f64_init(void) {
    g_f64Pool = BitPool(8, 1024);
    return BitPool_live(&g_f64Pool);
}

void f64_shutdown(void) {
    BitPool_shutdown(&g_f64Pool);
}

void *f64_alloc(void) {
    return BitPool_alloc(&g_f64Pool, ID_f64);
}

void *f64_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(d64);
    // For compound types, elem_size 8 already accounts for stride, but sizeof(d64) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 8 != sizeof(d64))
        Bytes = count * 8;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_f64), Bytes);
}

void f64_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_f64Pool, ptr)) BitPool_free(&g_f64Pool, ptr);
    else Memory_free(ptr);
}

d64 f64_get(void *ptr) {
    if (!ptr)
        return (d64) 0;
    return *(d64*) ptr;
}

void f64_set(void *ptr, d64 value) {
    if (!ptr)
        return;
    *(d64*) ptr = value;
}

bool f64_compareAndSet(void *ptr, d64 expected, d64 value) {
    if (!ptr)
        return false;
    int64_t expBits;
    int64_t valBits;
    memcpy(&expBits, &expected, sizeof(expBits));
    memcpy(&valBits, &value, sizeof(valBits));
    return __atomic_compare_exchange_n((int64_t*) ptr, &expBits, valBits, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 f64_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_f64Pool, ptr)) return BitPool_type(&g_f64Pool, ptr);
    return Memory_type(ptr);
}

bytes f64_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_f64Pool, ptr)) return BitPool_length(&g_f64Pool, ptr);
    return Memory_length(ptr);
}

void *f64_allocWithValue(d64 value) {
    void *ptr = f64_alloc();
    if (ptr) f64_set(ptr, value);
    return ptr;
}
