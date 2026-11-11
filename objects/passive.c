#include "objects/passive.h"

#include <string.h>

#include "nio/mem.h"
#include "oop/type.h"
#include "util/annotation.h"

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
 *     void *userdata;            // opaque callback context
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
    void         *userdata;
};

Passive *Passive_3(PassiveGetter getter, PassiveSetter setter, void *userdata) {
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_PASSIVE) | WRAP_PROACTIVE;
    Passive *passive = (Passive*) Memory_alloc(type, sizeof(Passive));
    if (!passive)
        return nullptr;
    (*passive).cachedValue = 0;
    (*passive).getter = getter;
    (*passive).setter = setter;
    (*passive).userdata = userdata;
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
void Passive_free(Passive *passive) {
    if (!passive) return;
    Memory_free(passive);
}

u64 Passive_get(Passive *passive) {
    if (!passive) return 0;
    if ((*passive).getter) {
        return (*passive).getter((*passive).userdata);
    }
    return (*passive).cachedValue;
}

void Passive_set(Passive *passive, u64 value) {
    if (!passive) return;
    (*passive).cachedValue = value;
    if ((*passive).setter) {
        (*passive).setter(value, (*passive).userdata);
    }
}
