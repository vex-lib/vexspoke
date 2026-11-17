#include "vex.h"
#include "deferred/dispatch.h"

#include <stdlib.h>

;;DEFINITION
/** The deferred-behavior mailbox. It exists so a caller can hand work to another
 * thread without owning that thread: post appends one (fn, context) slot under a
 * single-try lock, and drain swaps the pending buffer for a reusable one under
 * the lock before invoking its snapshot outside it. Admission never spins and
 * storage grows by checked doubling. Posts made during delivery stay pending for
 * the next pass, the atomic draining flag rejects nested or concurrent drains,
 * and payloads are borrowed and opaque — the mailbox schedules nothing and owns
 * nothing it delivers. */

;;OVERVIEW
/**
 * CLASS: Dispatch — L2 Behavior, MPSC mailbox with one consumer.
 * FIELDS: SpinLock lock; DispatchItem *items; bytes count, cap;
 * DispatchItem *batch; bytes batchCap; atomic_bool draining;
 * atomic_size_t pendingCount; _Atomic u64 drainedCount.
 * SLOT: DispatchItem { DispatchFn fn; any context; } (behaviorless).
 * CONSTRUCTOR: Dispatch() / Dispatch_0().
 * CORE: Dispatch_drain, Dispatch_free. SETTER: Dispatch_post.
 * GETTERS: Dispatch_pending, Dispatch_getDrainedCount. HELPERS: none.
 *
 * Single-try admission never spins. Pending storage grows with checked
 * doubling. Drain swaps reusable buffers under the lock, then invokes its
 * snapshot outside it. Posts made during delivery remain pending next pass.
 * Atomic draining rejects nested/concurrent drains; telemetry is read-only.
 * Payloads are borrowed and opaque. No scheduling, coalescing or ownership
 * transfer of context. Free requires external exclusion of all API callers.
 */

;;GETTER
static inline any DispatchItem_context(const DispatchItem *s) { return (*s).context; }
;;GETTER
static inline any *DispatchItem_contextMut(DispatchItem *s) { return &(*s).context; }
;;SETTER
static inline fn DispatchItem_setContext(DispatchItem *s, any v) { (*s).context = v; }
;;GETTER
static inline DispatchFn DispatchItem_fn(const DispatchItem *s) { return (*s).fn; }
;;GETTER
static inline DispatchFn *DispatchItem_fnMut(DispatchItem *s) { return &(*s).fn; }
;;SETTER
static inline fn DispatchItem_setFn(DispatchItem *s, DispatchFn v) { (*s).fn = v; }

;;GETTER
static inline DispatchItem * Dispatch_batch(const Dispatch *s) { return (*s).batch; }
;;GETTER
static inline DispatchItem * *Dispatch_batchMut(Dispatch *s) { return &(*s).batch; }
;;SETTER
static inline fn Dispatch_setBatch(Dispatch *s, DispatchItem * v) { (*s).batch = v; }
;;GETTER
static inline bytes Dispatch_batchCap(const Dispatch *s) { return (*s).batchCap; }
;;GETTER
static inline bytes *Dispatch_batchCapMut(Dispatch *s) { return &(*s).batchCap; }
;;SETTER
static inline fn Dispatch_setBatchCap(Dispatch *s, bytes v) { (*s).batchCap = v; }
;;GETTER
static inline bytes Dispatch_cap(const Dispatch *s) { return (*s).cap; }
;;GETTER
static inline bytes *Dispatch_capMut(Dispatch *s) { return &(*s).cap; }
;;SETTER
static inline fn Dispatch_setCap(Dispatch *s, bytes v) { (*s).cap = v; }
;;GETTER
static inline bytes Dispatch_count(const Dispatch *s) { return (*s).count; }
;;GETTER
static inline bytes *Dispatch_countMut(Dispatch *s) { return &(*s).count; }
;;SETTER
static inline fn Dispatch_setCount(Dispatch *s, bytes v) { (*s).count = v; }
;;GETTER
static inline _Atomic u64 Dispatch_drainedCount(const Dispatch *s) { return (*s).drainedCount; }
;;GETTER
static inline _Atomic u64 *Dispatch_drainedCountMut(Dispatch *s) { return &(*s).drainedCount; }
;;SETTER
static inline fn Dispatch_setDrainedCount(Dispatch *s, _Atomic u64 v) { (*s).drainedCount = v; }
;;GETTER
static inline atomic_bool Dispatch_draining(const Dispatch *s) { return (*s).draining; }
;;GETTER
static inline atomic_bool *Dispatch_drainingMut(Dispatch *s) { return &(*s).draining; }
;;SETTER
static inline fn Dispatch_setDraining(Dispatch *s, atomic_bool v) { (*s).draining = v; }
;;GETTER
static inline DispatchItem * Dispatch_items(const Dispatch *s) { return (*s).items; }
;;GETTER
static inline DispatchItem * *Dispatch_itemsMut(Dispatch *s) { return &(*s).items; }
;;SETTER
static inline fn Dispatch_setItems(Dispatch *s, DispatchItem * v) { (*s).items = v; }
;;GETTER
static inline SpinLock Dispatch_lock(const Dispatch *s) { return (*s).lock; }
;;GETTER
static inline SpinLock *Dispatch_lockMut(Dispatch *s) { return &(*s).lock; }
;;SETTER
static inline fn Dispatch_setLock(Dispatch *s, SpinLock v) { (*s).lock = v; }
;;GETTER
static inline atomic_size_t Dispatch_pendingCount(const Dispatch *s) { return (*s).pendingCount; }
;;GETTER
static inline atomic_size_t *Dispatch_pendingCountMut(Dispatch *s) { return &(*s).pendingCount; }
;;SETTER
static inline fn Dispatch_setPendingCount(Dispatch *s, atomic_size_t v) { (*s).pendingCount = v; }

Dispatch *Dispatch_0() {
    Dispatch *self = (Dispatch*) calloc(1, sizeof(Dispatch));
    if (!self)
        return nullptr;
    *Dispatch_lockMut(self) = (SpinLock) {0};
    atomic_init(Dispatch_drainingMut(self), false);
    atomic_init(Dispatch_pendingCountMut(self), 0);
    atomic_init(Dispatch_drainedCountMut(self), 0);
    return self;
}

bool Dispatch_post(Dispatch *self, DispatchFn fn, any context) {
    if (!self || !fn)
        return false;
    if (!SpinLock_tryLock(Dispatch_lockMut(self)))
        return false;
    if (Dispatch_count(self) == Dispatch_cap(self)) {
        if (Dispatch_cap(self) > SIZE_MAX / sizeof(DispatchItem) / 2) {
            SpinLock_unlock(Dispatch_lockMut(self));
            return false;
        }
        bytes cap = Dispatch_cap(self) ? Dispatch_cap(self) * 2 : 8;
        DispatchItem *items = (DispatchItem*) realloc(Dispatch_items(self), cap * sizeof(DispatchItem));
        if (!items) {
            SpinLock_unlock(Dispatch_lockMut(self));
            return false;
        }
        Dispatch_setItems(self, items);
        Dispatch_setCap(self, cap);
    }
    DispatchItem *item = &Dispatch_itemsMut(self)[Dispatch_count(self)];
    DispatchItem_setFn(item, fn);
    DispatchItem_setContext(item, context);
    Dispatch_setCount(self, Dispatch_count(self) + 1u);
    atomic_store_explicit(Dispatch_pendingCountMut(self), Dispatch_count(self), memory_order_relaxed);
    SpinLock_unlock(Dispatch_lockMut(self));
    return true;
}

bool Dispatch_drain(Dispatch *self) {
    if (!self)
        return false;
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(Dispatch_drainingMut(self), &expected, true,
                                                 memory_order_acquire, memory_order_relaxed))
        return false;
    if (!SpinLock_tryLock(Dispatch_lockMut(self))) {
        atomic_store_explicit(Dispatch_drainingMut(self), false, memory_order_release);
        return false;
    }
    bytes n = Dispatch_count(self);
    if (n != 0) {
        DispatchItem *batch = Dispatch_items(self);
        bytes cap = Dispatch_cap(self);
        Dispatch_setItems(self, Dispatch_batch(self));
        Dispatch_setCap(self, Dispatch_batchCap(self));
        Dispatch_setBatch(self, batch);
        Dispatch_setBatchCap(self, cap);
        Dispatch_setCount(self, 0);
        atomic_store_explicit(Dispatch_pendingCountMut(self), 0, memory_order_relaxed);
    }
    SpinLock_unlock(Dispatch_lockMut(self));
    for (bytes i = 0; i < n; i++) {
        DispatchItem *item = &Dispatch_batchMut(self)[i];
        DispatchItem_fn(item)(DispatchItem_context(item));
        atomic_fetch_add_explicit(Dispatch_drainedCountMut(self), 1, memory_order_relaxed);
    }
    atomic_store_explicit(Dispatch_drainingMut(self), false, memory_order_release);
    return true;
}

bool Dispatch_free(Dispatch *self) {
    if (!self)
        return false;
    if (atomic_load_explicit(Dispatch_drainingMut(self), memory_order_acquire))
        return false;
    if (Dispatch_count(self) > 0)
        fprintf(stderr, "dispatch: dropping %zu undelivered item(s) at free\n", Dispatch_count(self));
    free(Dispatch_items(self));
    free(Dispatch_batch(self));
    free(self);
    return true;
}

bytes Dispatch_pending(const Dispatch *self) {
    return self ? atomic_load_explicit(Dispatch_pendingCountMut(self), memory_order_relaxed) : 0;
}

u64 Dispatch_getDrainedCount(const Dispatch *self) {
    return self ? atomic_load_explicit(Dispatch_drainedCountMut(self), memory_order_relaxed) : 0;
}
