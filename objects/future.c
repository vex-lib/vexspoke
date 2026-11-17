#include "vex.h"
#include "re.h"
#include "objects/future.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Future
 * ============================================================================
 * Asynchronous single-assignment Future wrapper: an atomic isGiven flag plus
 * a uint64 payload. Future_setDesiredValue fulfills the future exactly once
 * via a CAS on the flag (acq_rel publish), so the first writer wins and later
 * writers are rejected; readers observe the payload with acquire ordering.
 * Future_2 builds arrays of futures from an init sample or zeroed memory.
 * Instances are memory-arena allocated with the WRAP2_FUTURE type id.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Future (objects/future.c)
 * ============================================================================
 * Asynchronous single-assignment Future object wrapper.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   Future {
 *     atomic_bool isGiven;       // fulfillment flag (single-assignment)
 *     u64 value;            // resolved payload (0 until given)
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Future_0()
 *   - Future_2(init, count)
 *
 * Core Functions:
 *   - Future_free(future)
 *
 * Setters:
 *   - Future_setDesiredValue(future, value)
 *
 * Getters:
 *   - Future_isGiven(future)
 *   - Future_get(future)
 * ============================================================================
 */


// future.c — Asynchronous single-assignment Future object wrapper implementation.
typedef struct Future {
    atomic_bool isGiven;
    u64    value;
} Future;

;;GETTER
static inline atomic_bool Future_isGiven(const Future *s) { return (*s).isGiven; }
;;GETTER
static inline atomic_bool *Future_isGivenMut(Future *s) { return &(*s).isGiven; }
;;SETTER
static inline fn Future_setIsGiven(Future *s, atomic_bool v) { (*s).isGiven = v; }
;;GETTER
static inline u64 Future_value(const Future *s) { return (*s).value; }
;;GETTER
static inline u64 *Future_valueMut(Future *s) { return &(*s).value; }
;;SETTER
static inline fn Future_setValue(Future *s, u64 v) { (*s).value = v; }


// Allocates one unresolved future with a zero payload.
Future *Future_0() {
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_FUTURE) | WRAP2_FUTURE;
    Future *future = Memory_alloc(type, sizeof(Future));
    if (!future)
        return nullptr;
    atomic_init(Future_isGivenMut(future), false);
    Future_setValue(future, 0);
    return future;
}


// Allocates count future records, copying init when supplied or zeroing otherwise.
Future *Future_2(const Future *init, bytes count) {
    if (count == 0) return nullptr;
    Future *p = (Future*) Memory_alloc(TYPE_FUTURE_ARRAY, sizeof(Future) * count);
    if (!p) return nullptr;
    if (init) {
        for (bytes i = 0; i < count; i++) p[i] = *init;
    } else {
        memset(p, 0, sizeof(Future) * count);
    }
    return p;
}
// Releases the allocation through the engine-owned Memory ABI.
fn Future_free(Future *future) {
    if (!future) return;
    Memory_free(future);
}

// Reports whether the future's atomic fulfillment flag is set.
bool Future_isGiven(const Future *future) {
    if (!future) return false;
    return atomic_load_explicit(Future_isGivenMut(future), memory_order_acquire);
}

// Returns the payload, or zero for a null future.
u64 Future_get(const Future *future) {
    if (!future) return 0;
    return Future_value(future);
}

// Attempts one single-assignment fulfillment; returns false if already fulfilled.
bool Future_setDesiredValue(Future *future, u64 value) {
    if (!future) return false;
    bool expected = false;
    if (atomic_compare_exchange_strong_explicit(
            Future_isGivenMut(future), &expected, true,
            memory_order_acq_rel, memory_order_acquire)) {
        Future_setValue(future, value);
        return true;
    }
    return false;
}
