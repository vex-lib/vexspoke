#include "vex.h"
#include "re.h"
#include "objects/passive.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Passive
 * ============================================================================
 * Lazy-evaluated/computed object wrapper: a memoized cachedValue plus
 * getter/setter callbacks and opaque userdata, so a value can be computed on
 * first access and written back through the setter. Exists because the
 * relational object model needs computed properties that behave like plain
 * fields. memory: arena-allocated via Memory_alloc with the PASSIVE type id;
 * Passive_2 builds arrays of count copies. Lifetime: memory arena;
 * Passive_free returns the block.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Passive (objects/passive.c)
 * ============================================================================
 * Lazy-evaluated / computed Passive object wrapper.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   Passive {
 *     u64 cachedValue;      // memoized lazy value
 *     PassiveGetter getter;      // lazy compute callback
 *     PassiveSetter setter;      // optional write-back callback
 *     any userdata;            // opaque callback context
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Passive_3(getter, setter, userdata)
 *   - Passive_2(init, count)
 *
 * Core Functions:
 *   - Passive_free(passive)
 *
 * Setters:
 *   - Passive_set(passive, value)
 *
 * Getters:
 *   - Passive_get(passive)
 * ============================================================================
 */


// passive.c — Lazy-evaluated / computed Passive object wrapper implementation.

struct Passive {
    u64      cachedValue;
    PassiveGetter getter;
    PassiveSetter setter;
    any userdata;
};

;;GETTER
static inline u64 Passive_cachedValue(const Passive *s) { return (*s).cachedValue; }
;;GETTER
static inline u64 *Passive_cachedValueMut(Passive *s) { return &(*s).cachedValue; }
;;SETTER
static inline fn Passive_setCachedValue(Passive *s, u64 v) { (*s).cachedValue = v; }
;;GETTER
static inline PassiveGetter Passive_getter(const Passive *s) { return (*s).getter; }
;;GETTER
static inline PassiveGetter *Passive_getterMut(Passive *s) { return &(*s).getter; }
;;SETTER
static inline fn Passive_setGetter(Passive *s, PassiveGetter v) { (*s).getter = v; }
;;GETTER
static inline PassiveSetter Passive_setter(const Passive *s) { return (*s).setter; }
;;GETTER
static inline PassiveSetter *Passive_setterMut(Passive *s) { return &(*s).setter; }
;;SETTER
static inline fn Passive_setSetter(Passive *s, PassiveSetter v) { (*s).setter = v; }
;;GETTER
static inline any Passive_userdata(const Passive *s) { return (*s).userdata; }
;;GETTER
static inline any *Passive_userdataMut(Passive *s) { return &(*s).userdata; }
;;SETTER
static inline fn Passive_setUserdata(Passive *s, any v) { (*s).userdata = v; }


Passive *Passive_3(PassiveGetter getter, PassiveSetter setter, any userdata) {
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_PASSIVE) | WRAP_PROACTIVE;
    Passive *passive = (Passive*) Memory_alloc(type, sizeof(Passive));
    if (!passive)
        return nullptr;
    Passive_setCachedValue(passive, 0);
    Passive_setGetter(passive, getter);
    Passive_setSetter(passive, setter);
    Passive_setUserdata(passive, userdata);
    return passive;
}


Passive *Passive_2(const Passive *init, bytes count) {
    if (count == 0) return nullptr;
    Passive *p = (Passive*) Memory_alloc(TYPE_PASSIVE_ARRAY, sizeof(Passive) * count);
    if (!p) return nullptr;
    if (init) {
        for (bytes i = 0; i < count; i++) p[i] = *init;
    } else {
        memset(p, 0, sizeof(Passive) * count);
    }
    return p;
}
fn Passive_free(Passive *passive) {
    if (!passive) return;
    Memory_free(passive);
}

u64 Passive_get(Passive *passive) {
    if (!passive) return 0;
    if (Passive_getter(passive)) {
        return Passive_getter(passive)(Passive_userdata(passive));
    }
    return Passive_cachedValue(passive);
}

fn Passive_set(Passive *passive, u64 value) {
    if (!passive) return;
    Passive_setCachedValue(passive, value);
    if (Passive_setter(passive)) {
        Passive_setter(passive)(value, Passive_userdata(passive));
    }
}
