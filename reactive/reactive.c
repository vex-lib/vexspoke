// reactive/reactive.c — the one reactive engine.

#include "vex.h"
#include "reactive/reactive.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Reactive
 * ============================================================================
 * The reactive engine: one atomic word + a shadow + a dirty flag + three
 * observer lists (onSet / onChanged / onGet / onNullptr). Type-agnostic — every typed
 * reactive embeds it. Reactive_store (any thread) moves the word and marks dirty
 * and never fires; Reactive_drain (owner thread) coalesces the pending writes
 * into one batch and fires onSet then onChanged, with old = shadow and new =
 * value, so a change is exactly one compare and both endpoints are exact.
 *
 * The observer lists are arena-backed, allocated lazily (an unwatched reactive
 * costs no list), and d64 on demand (the Dynamic Scalability & Anti-Hardcoding
 * Law). A removed observer leaves a reusable tombstone, so a mid-fire removal
 * never invalidates a fire.
 *
 * Lifetime: reactive_init for an embedded engine (inside a typed facade), or
 * Reactive_1/_2 for an arena one. reactiveShutdown releases
 * the lists; reactiveFree additionally releases the block.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Reactive (reactive/reactive.c)
 * ============================================================================
 * one atomic word + shadow + dirty + three observer lists.
 *
 * STRUCT FIELDS (Mirroring reactive/reactive.h):
 * ----------------------------------------------------------------------------
 *   Reactive {
 *     _Atomic(pointer) value; // the live word
 *     pointer shadow;         // owner-affine: the last drained word
 *     _Atomic bool dirty;       // a write awaits the owner's drain
 *     bool active;              // runtime-active flag
 *     bool deliverBound;        // true: own hook; false: inherit the default
 *     ReactiveDeliverFn deliver; // auto-delivery on store; nullptr = manual
 *     ReactiveValueFn valueOf;   // formats the word as text (typed), or nullptr
 *     ReactiveObserverList *onSet;     // fires once per drained batch
 *     ReactiveObserverList *onChanged; // fires when the drained word moved
 *     ReactiveObserverList *onGet;     // fires on each read (immediate)
 *     ReactiveObserverList *onNullptr; // fires when a drained word is 0
 *   }
 *
 * PRIVATE HELPERS (kept file-local, pure data + list math only):
 * ----------------------------------------------------------------------------
 *   ReactiveObserver { any cb; any userdata; }        // SLOT RECORD
 *   ReactiveObserverList { ReactiveObserver *items; bytes count, cap; } // SLOT RECORD
 *   listEnsure(slot) / listAdd(list, cb, userdata) / listRemove(...) / listFree(list)
 *   fireSet/fireChanged (inline over the lists)           // the fire walks
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h) Reactive_init, Reactive_1, Reactive_2, Reactive_free
 * Public Core Functions: (.h) Reactive_load, Reactive_store, Reactive_drain
 * Public Observers: (.h) Reactive_watchSet/_watchChanged/_watchGet/_watchNullptr (+ unwatch*)
 * Public Getters: (.h) Reactive_observerCount, Reactive_isDirty, Reactive_isActive
 * ============================================================================
 */


;;GETTER
static inline bool Reactive_active(const Reactive *s) { return (*s).active; }
;;SETTER
static inline fn Reactive_setActive(Reactive *s, bool v) { (*s).active = v; }
;;GETTER
static inline ReactiveDeliverFn Reactive_deliver(const Reactive *s) { return (*s).deliver; }
;;SETTER
static inline fn Reactive_setDeliver(Reactive *s, ReactiveDeliverFn v) { (*s).deliver = v; }
;;GETTER
static inline bool Reactive_deliverBound(const Reactive *s) { return (*s).deliverBound; }
;;SETTER
static inline fn Reactive_setDeliverBound(Reactive *s, bool v) { (*s).deliverBound = v; }
;;GETTER
static inline const _Atomic bool *Reactive_dirtyPtr(const Reactive *s) { return &(*s).dirty; }
;;GETTER
static inline _Atomic bool *Reactive_dirtyMut(Reactive *s) { return &(*s).dirty; }
;;GETTER
static inline ReactiveObserverList * Reactive_onChanged(const Reactive *s) { return (*s).onChanged; }
;;GETTER
static inline ReactiveObserverList * *Reactive_onChangedMut(Reactive *s) { return &(*s).onChanged; }
;;SETTER
static inline fn Reactive_setOnChanged(Reactive *s, ReactiveObserverList * v) { (*s).onChanged = v; }
;;GETTER
static inline ReactiveObserverList * Reactive_onGet(const Reactive *s) { return (*s).onGet; }
;;GETTER
static inline ReactiveObserverList * *Reactive_onGetMut(Reactive *s) { return &(*s).onGet; }
;;SETTER
static inline fn Reactive_setOnGet(Reactive *s, ReactiveObserverList * v) { (*s).onGet = v; }
;;GETTER
static inline ReactiveObserverList * Reactive_onNullptr(const Reactive *s) { return (*s).onNullptr; }
;;GETTER
static inline ReactiveObserverList * *Reactive_onNullptrMut(Reactive *s) { return &(*s).onNullptr; }
;;SETTER
static inline fn Reactive_setOnNullptr(Reactive *s, ReactiveObserverList * v) { (*s).onNullptr = v; }
;;GETTER
static inline ReactiveObserverList * Reactive_onSet(const Reactive *s) { return (*s).onSet; }
;;GETTER
static inline ReactiveObserverList * *Reactive_onSetMut(Reactive *s) { return &(*s).onSet; }
;;SETTER
static inline fn Reactive_setOnSet(Reactive *s, ReactiveObserverList * v) { (*s).onSet = v; }
;;GETTER
static inline pointer Reactive_shadow(const Reactive *s) { return (*s).shadow; }
;;SETTER
static inline fn Reactive_setShadow(Reactive *s, pointer v) { (*s).shadow = v; }
;;GETTER
static inline _Atomic(pointer) *Reactive_valueMut(Reactive *s) { return &(*s).value; }
;;GETTER
static inline ReactiveValueFn Reactive_valueOfRaw(const Reactive *s) { return (*s).valueOf; }
;;SETTER
static inline fn Reactive_setValueOfRaw(Reactive *s, ReactiveValueFn v) { (*s).valueOf = v; }

// SLOT RECORD: one observer entry (a callback + its context).
typedef struct ReactiveObserver {
    any cb;       // the typed callback (cast per list)
    any userdata; // per-observer context
} ReactiveObserver;

// SLOT RECORD: one event's observer list (arena-backed, doubling).
struct ReactiveObserverList {
    ReactiveObserver *items;
    bytes count;
    bytes cap;
};

;;GETTER
static inline bytes ReactiveObserverList_cap(const ReactiveObserverList *s) { return (*s).cap; }
;;SETTER
static inline fn ReactiveObserverList_setCap(ReactiveObserverList *s, bytes v) { (*s).cap = v; }
;;GETTER
static inline bytes ReactiveObserverList_count(const ReactiveObserverList *s) { return (*s).count; }
;;SETTER
static inline fn ReactiveObserverList_setCount(ReactiveObserverList *s, bytes v) { (*s).count = v; }
;;GETTER
static inline ReactiveObserver * ReactiveObserverList_items(const ReactiveObserverList *s) { return (*s).items; }
;;SETTER
static inline fn ReactiveObserverList_setItems(ReactiveObserverList *s, ReactiveObserver * v) { (*s).items = v; }


#define REACTIVE_ARRAY_TYPE (SUGAR_VEX | PROJ_VEXSPOKE | FORM_SINGLETON | WRAP_REACTIVE | ID_REACTIVE)

// Mint a list on first bind (an unwatched reactive costs no list).
static ReactiveObserverList *listEnsure(ReactiveObserverList **slot) {
    if (*slot != nullptr)
        return *slot;
    ReactiveObserverList *list = (ReactiveObserverList*) Memory_alloc(REACTIVE_ARRAY_TYPE, sizeof(ReactiveObserverList));
    if (list == nullptr)
        return nullptr;
    ReactiveObserverList_setItems(list, nullptr);
    ReactiveObserverList_setCount(list, 0);
    ReactiveObserverList_setCap(list, 0);
    *slot = list;
    return list;
}

// Add (or reuse a tombstone slot for) an observer. Returns false on null cb/OOM.
static bool listAdd(ReactiveObserverList **slot, any cb, any userdata) {
    if (cb == nullptr)
        return false;
    ReactiveObserverList *list = listEnsure(slot);
    if (list == nullptr)
        return false;
    for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
        if (ReactiveObserverList_items(list)[i].cb == nullptr) { // reuse a tombstone
            ReactiveObserverList_items(list)[i].cb = cb;
            ReactiveObserverList_items(list)[i].userdata = userdata;
            return true;
        }
    }
    if (ReactiveObserverList_count(list) >= ReactiveObserverList_cap(list)) {
        bytes newCap = (ReactiveObserverList_cap(list) == 0) ? 4 : ReactiveObserverList_cap(list) * 2;
        ReactiveObserver *nb = (ReactiveObserver*) Memory_realloc(ReactiveObserverList_items(list), newCap * sizeof(ReactiveObserver));
        if (nb == nullptr)
            return false;
        ReactiveObserverList_setItems(list, nb);
        ReactiveObserverList_setCap(list, newCap);
    }
    ReactiveObserverList_items(list)[ReactiveObserverList_count(list)].cb = cb;
    ReactiveObserverList_items(list)[ReactiveObserverList_count(list)].userdata = userdata;
    ReactiveObserverList_setCount(list, ReactiveObserverList_count(list) + 1u);
    return true;
}

// Remove an observer (leaves a reusable tombstone). Returns false if absent.
static bool listRemove(ReactiveObserverList **slot, any cb, any userdata) {
    if (cb == nullptr || *slot == nullptr)
        return false;
    ReactiveObserverList *list = *slot;
    for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
        if (ReactiveObserverList_items(list)[i].cb == cb && ReactiveObserverList_items(list)[i].userdata == userdata) {
            ReactiveObserverList_items(list)[i].cb = nullptr;
            ReactiveObserverList_items(list)[i].userdata = nullptr;
            return true;
        }
    }
    return false;
}

static fn listFree(ReactiveObserverList **slot) {
    if (*slot == nullptr)
        return;
    if (ReactiveObserverList_items(*slot) != nullptr)
        Memory_free(ReactiveObserverList_items(*slot));
    Memory_free(*slot);
    *slot = nullptr;
}

// The process-wide default delivery hook. Unset (nullptr) means the engine keeps
// its original contract: store marks dirty, the caller/owner drains by hand.
// thread/reactive.c sets this to its wake so a plain set self-delivers.
static ReactiveDeliverFn s_defaultDelivery = nullptr;

// CONSTRUCTORS

bool Reactive_init(Reactive *self, pointer initialWord) {
    if (self == nullptr)
        return false;
    atomic_init(Reactive_valueMut(self), initialWord);
    Reactive_setShadow(self, initialWord); // shadow starts equal: the first drain is silent
    atomic_init(Reactive_dirtyMut(self), false);
    Reactive_setActive(self, true);
    Reactive_setDeliverBound(self, false); // inherit the process default until bound
    Reactive_setDeliver(self, nullptr);
    Reactive_setValueOf(self, nullptr);    // hex fallback until a typed stample binds one
    Reactive_setOnSet(self, nullptr);
    Reactive_setOnChanged(self, nullptr);
    Reactive_setOnGet(self, nullptr);
    Reactive_setOnNullptr(self, nullptr);
    return true;
}

Reactive *Reactive_1(pointer initialWord) {
    Reactive *reactive = (Reactive*) Memory_alloc(REACTIVE_ARRAY_TYPE, sizeof(Reactive));
    if (reactive == nullptr)
        return nullptr;
    Reactive_init(reactive, initialWord);
    return reactive;
}

Reactive *Reactive_2(const Reactive *init, bytes count) {
    if (count == 0)
        return nullptr;
    Reactive *p = (Reactive*) Memory_alloc(TYPE_REACTIVE_ARRAY, sizeof(Reactive) * count);
    if (p == nullptr)
        return nullptr;
    pointer seed = (init != nullptr) ? Reactive_load((Reactive*) init) : 0u;
    for (bytes i = 0; i < count; i++)
        Reactive_init(&p[i], seed);
    return p;
}

fn Reactive_shutdown(Reactive *self) {
    if (self == nullptr)
        return;
    listFree(Reactive_onSetMut(self));
    listFree(Reactive_onChangedMut(self));
    listFree(Reactive_onGetMut(self));
    listFree(Reactive_onNullptrMut(self));
    Reactive_setActive(self, false);
}

fn Reactive_free(Reactive *self) {
    if (self == nullptr)
        return;
    Reactive_shutdown(self);
    Memory_free(self);
}

// CORE FUNCTIONS

pointer Reactive_load(Reactive *self) {
    if (self == nullptr)
        return 0u;
    pointer value = atomic_load_explicit(Reactive_valueMut(self), memory_order_acquire);
    // onGet is the ONE-FIRE READ: it fires immediately, on the reader's thread,
    // every time the value is read. No observers bound -> a bare atomic load, so
    // the hot path stays cheap.
    ReactiveObserverList *list = Reactive_onGet(self);
    if (list != nullptr) {
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
            ReactiveObserver o = ReactiveObserverList_items(list)[i];
            if (o.cb != nullptr)
                ((ReactiveGetFn) o.cb)(self, value, o.userdata);
        }
    }
    return value;
}

fn Reactive_store(Reactive *self, pointer word) {
    if (self == nullptr)
        return;
    // Any thread: move the word, mark dirty, fire nothing.
    atomic_store_explicit(Reactive_valueMut(self), word, memory_order_release);
    atomic_store_explicit(Reactive_dirtyMut(self), true, memory_order_release);
    // The delivery hook only SCHEDULES an owner drain (thread/reactive.c); the
    // observers still run on the owner's thread, never here.
    ReactiveDeliverFn fn = Reactive_deliverBound(self) ? Reactive_deliver(self) : s_defaultDelivery;
    if (fn != nullptr)
        fn(self);
}

fn Reactive_setDelivery(Reactive *self, ReactiveDeliverFn deliver) {
    if (self == nullptr)
        return;
    Reactive_setDeliver(self, deliver);
    Reactive_setDeliverBound(self, true); // explicit (nullptr means "none", not "inherit")
}

fn Reactive_clearDelivery(Reactive *self) {
    if (self == nullptr)
        return;
    Reactive_setDeliver(self, nullptr);
    Reactive_setDeliverBound(self, false); // fall back to the process default
}

ReactiveDeliverFn Reactive_getDelivery(const Reactive *self) {
    return self ? Reactive_deliver(self) : nullptr;
}

fn Reactive_setDefaultDelivery(ReactiveDeliverFn deliver) {
    s_defaultDelivery = deliver;
}

ReactiveDeliverFn Reactive_getDefaultDelivery() {
    return s_defaultDelivery;
}

fn Reactive_setValueOf(Reactive *self, ReactiveValueFn valueOf) {
    if (self == nullptr)
        return;
    Reactive_setValueOfRaw(self, valueOf);
}

const char *Reactive_valueOf(Reactive *self, char *out, bytes cap) {
    if (out == nullptr || cap == 0u)
        return out;
    if (self == nullptr) {
        out[0] = '\0';
        return out;
    }
    pointer word = atomic_load_explicit(Reactive_valueMut(self), memory_order_acquire);
    if (Reactive_valueOfRaw(self) != nullptr)
        Reactive_valueOfRaw(self)(word, out, cap);
    else
        snprintf(out, cap, "%llu", (u64) word);
    return out;
}

bool Reactive_drain(Reactive *self) {
    if (self == nullptr)
        return false;
    // Consume the dirty flag first: the acq_rel exchange pairs with the writer's
    // release store, so the load below observes the written word.
    if (!atomic_exchange_explicit(Reactive_dirtyMut(self), false, memory_order_acq_rel))
        return false; // nothing written since the last drain
    pointer value = atomic_load_explicit(Reactive_valueMut(self), memory_order_acquire);
    pointer old = Reactive_shadow(self);
    // onSet fires once for the whole batch (coalesced: last value wins).
    if (Reactive_onSet(self) != nullptr) {
        ReactiveObserverList *list = Reactive_onSet(self);
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
            ReactiveObserver o = ReactiveObserverList_items(list)[i];
            if (o.cb != nullptr)
                ((ReactiveSetFn) o.cb)(self, value, o.userdata);
        }
    }
    // onChanged fires only on a real move; latch the shadow before firing so a
    // re-entrant drain from a callback is a no-op.
    if (value != old) {
        Reactive_setShadow(self, value);
        if (Reactive_onChanged(self) != nullptr) {
            ReactiveObserverList *list = Reactive_onChanged(self);
            for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
                ReactiveObserver o = ReactiveObserverList_items(list)[i];
                if (o.cb != nullptr)
                    ((ReactiveChangedFn) o.cb)(self, old, value, o.userdata);
            }
        }
    }
    // onNullptr fires when the drained word is empty (0 = no value bound).
    if (value == 0u && Reactive_onNullptr(self) != nullptr) {
        ReactiveObserverList *list = Reactive_onNullptr(self);
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++) {
            ReactiveObserver o = ReactiveObserverList_items(list)[i];
            if (o.cb != nullptr)
                ((ReactiveNullptrFn) o.cb)(self, o.userdata);
        }
    }
    return true;
}

// OBSERVERS

bool Reactive_watchSet(Reactive *self, ReactiveSetFn cb, any userdata) {
    return self ? listAdd(Reactive_onSetMut(self), (any) cb, userdata) : false;
}

bool Reactive_unwatchSet(Reactive *self, ReactiveSetFn cb, any userdata) {
    return self ? listRemove(Reactive_onSetMut(self), (any) cb, userdata) : false;
}

bool Reactive_watchChanged(Reactive *self, ReactiveChangedFn cb, any userdata) {
    return self ? listAdd(Reactive_onChangedMut(self), (any) cb, userdata) : false;
}

bool Reactive_unwatchChanged(Reactive *self, ReactiveChangedFn cb, any userdata) {
    return self ? listRemove(Reactive_onChangedMut(self), (any) cb, userdata) : false;
}

bool Reactive_watchGet(Reactive *self, ReactiveGetFn cb, any userdata) {
    return self ? listAdd(Reactive_onGetMut(self), (any) cb, userdata) : false;
}

bool Reactive_unwatchGet(Reactive *self, ReactiveGetFn cb, any userdata) {
    return self ? listRemove(Reactive_onGetMut(self), (any) cb, userdata) : false;
}

bool Reactive_watchNullptr(Reactive *self, ReactiveNullptrFn cb, any userdata) {
    return self ? listAdd(Reactive_onNullptrMut(self), (any) cb, userdata) : false;
}

bool Reactive_unwatchNullptr(Reactive *self, ReactiveNullptrFn cb, any userdata) {
    return self ? listRemove(Reactive_onNullptrMut(self), (any) cb, userdata) : false;
}

// GETTERS

bytes Reactive_observerCount(const Reactive *self) {
    if (self == nullptr)
        return 0u;
    bytes n = 0u;
    const ReactiveObserverList *list = Reactive_onSet(self);
    if (list != nullptr) {
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++)
            if (ReactiveObserverList_items(list)[i].cb != nullptr) n++;
    }
    list = Reactive_onChanged(self);
    if (list != nullptr) {
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++)
            if (ReactiveObserverList_items(list)[i].cb != nullptr) n++;
    }
    list = Reactive_onGet(self);
    if (list != nullptr) {
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++)
            if (ReactiveObserverList_items(list)[i].cb != nullptr) n++;
    }
    list = Reactive_onNullptr(self);
    if (list != nullptr) {
        for (bytes i = 0; i < ReactiveObserverList_count(list); i++)
            if (ReactiveObserverList_items(list)[i].cb != nullptr) n++;
    }
    return n;
}

bool Reactive_isDirty(const Reactive *self) {
    if (self == nullptr)
        return false;
    return atomic_load_explicit((_Atomic bool*) Reactive_dirtyPtr(self), memory_order_acquire);
}

bool Reactive_isActive(const Reactive *self) {
    return self ? Reactive_active(self) : false;
}
