#include "primitive/brain.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Brain
 * ============================================================================
 * Brain primitive (Legacy: primitive/Brain.java): 16-bit bf3216 payloads
 * backed by a fixed 1024-slot BitPool for singletons and the memory arena for
 * arrays. Provides alloc/free, acquire/release get/set, compare-and-set,
 * type/length introspection, and lossy f32-to-bf3216 conversion with
 * round-to-nearest-even bias. Lives at R2 as a leaf primitive behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Brain (primitive/brain.c)
 * ============================================================================
 * Brain primitive (Legacy: primitive/Brain.java).
 *
 * STRUCT FIELDS: none — procedural (operates on BitPool/memory blocks of brain payloads)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Brain_init(void)
 *
 * Core Functions:
 *   - Brain_shutdown(void)
 *   - Brain_alloc(void)
 *   - Brain_allocArray(count)
 *   - Brain_free(ptr)
 *   - Brain_compareAndSet(ptr, expected, value)
 *   - Brain_type(ptr)
 *   - Brain_length(ptr)
 *   - Brain_f32ToBFloat16(value)
 *   - Brain_bFloat16ToFloat(bits)
 *   - Brain_allocWithValue(value)
 *
 * Setters:
 *   - Brain_set(ptr, value)
 *   - Brain_setFloat(ptr, value)
 *
 * Getters:
 *   - Brain_get(ptr)
 *   - Brain_getFloat(ptr)
 * ============================================================================
 */


BitPool g_brainPool;

bool Brain_init(void) {
    g_brainPool = BitPool(2, 1024);
    return BitPool_live(&g_brainPool);
}

void Brain_shutdown(void) {
    BitPool_shutdown(&g_brainPool);
}

void *Brain_alloc(void) {
    return BitPool_alloc(&g_brainPool, ID_BRAIN);
}

void *Brain_allocArray(bytes count) {
    if (count == 0)
        return nullptr;
    // Use memory arena for arrays — BitPool is fixed 1024 slots, not for large contiguous
    bytes Bytes = count * sizeof(u16);
    // For compound types, elem_size 2 already accounts for stride, but sizeof(u16) is placeholder
    // Use elem_size for true stride where c_type is int64 placeholder
    if (count > 1 && 2 != sizeof(u16))
        Bytes = count * 2;
    return Memory_alloc(Type_make(PROJ_VEXSPOKE, FORM_ARRAY, ID_BRAIN), Bytes);
}

void Brain_free(void *ptr) {
    if (!ptr)
        return;
    if (BitPool_contains(&g_brainPool, ptr)) BitPool_free(&g_brainPool, ptr);
    else Memory_free(ptr);
}

u16 Brain_get(void *ptr) {
    if (!ptr)
        return (u16) 0;
    return *(u16*) ptr;
}

void Brain_set(void *ptr, u16 value) {
    if (!ptr)
        return;
    *(u16*) ptr = value;
}

u16 Brain_f32ToBFloat16(f32 value) {
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    u32 lsb = (bits >> 16) & 1u;
    u32 roundingBias = 0x7FFFu + lsb;
    bits += roundingBias;
    return (u16)(bits >> 16);
}

f32 Brain_bFloat16ToFloat(u16 bits) {
    u32 expanded = ((u32) bits) << 16;
    f32 out;
    memcpy(&out, &expanded, sizeof(out));
    return out;
}

f32 Brain_getFloat(void *ptr) {
    return Brain_bFloat16ToFloat(Brain_get(ptr));
}

void Brain_setFloat(void *ptr, f32 value) {
    Brain_set(ptr, Brain_f32ToBFloat16(value));
}

bool Brain_compareAndSet(void *ptr, u16 expected, u16 value) {
    if (!ptr)
        return false;
    // fallback to simple CAS via __atomic_compare_exchange
    return __atomic_compare_exchange_n((u16*) ptr, &expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

u64 Brain_type(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_brainPool, ptr)) return BitPool_type(&g_brainPool, ptr);
    return Memory_type(ptr);
}

bytes Brain_length(void *ptr) {
    if (!ptr)
        return 0;
    if (BitPool_contains(&g_brainPool, ptr)) return BitPool_length(&g_brainPool, ptr);
    return Memory_length(ptr);
}

void *Brain_allocWithValue(u16 value) {
    void *ptr = Brain_alloc();
    if (ptr) Brain_set(ptr, value);
    return ptr;
}
