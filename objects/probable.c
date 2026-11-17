#include "vex.h"
#include "re.h"
#include "objects/probable.h"
#include "oop/vexspoke_type.h"
#include "util/random.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Probable
 * ============================================================================
 * Weighted-choice object: a payload pointer plus weight/total state used by
 * Random_sample for weighted selection. Probable_3 builds a single weighted
 * entry; Probable_2 fills an array from a template (or zeros it) for bulk
 * weighted tables. All storage is arena-allocated with TYPE_PROBABLE /
 * TYPE_PROBABLE_ARRAY identity; getters return safe defaults on null per the
 * Symmetric Getter/Setter Completeness Law.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Probable (objects/probable.c)
 * ============================================================================
 * Core subsystem implementation for Probable.
 *
 * STRUCT FIELDS (Mirroring objects/probable.h):
 * ----------------------------------------------------------------------------
 *   Probable {
 *     pointer object; // object pointer payload
 *     u32 weight; // weighted-choice state
 *     u32 total; // weighted-choice state
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Probable_3(object, weight, total)
 *   - Probable_2(init, count)
 *
 * Core Functions:
 *   - Probable_free(p)
 *   - Probable_object(p)
 *   - Probable_weight(p)
 *   - Probable_total(p)
 *
 * Setters:
 *   - Probable_setObject(p, object)
 *   - Probable_setWeight(p, weight)
 *   - Probable_setTotal(p, total)
 *
 * Getters:
 *   - Probable_get(p)
 * ============================================================================
 */



;;GETTER
static inline pointer Probable_object(const Probable *s) { return (*s).object; }
;;GETTER
static inline pointer *Probable_objectMut(Probable *s) { return &(*s).object; }
;;SETTER
static inline fn Probable_setObject(Probable *s, pointer v) { (*s).object = v; }
;;GETTER
static inline u32 Probable_total(const Probable *s) { return (*s).total; }
;;GETTER
static inline u32 *Probable_totalMut(Probable *s) { return &(*s).total; }
;;SETTER
static inline fn Probable_setTotal(Probable *s, u32 v) { (*s).total = v; }
;;GETTER
static inline u32 Probable_weight(const Probable *s) { return (*s).weight; }
;;GETTER
static inline u32 *Probable_weightMut(Probable *s) { return &(*s).weight; }
;;SETTER
static inline fn Probable_setWeight(Probable *s, u32 v) { (*s).weight = v; }

Probable *Probable_3(pointer object, u32 weight, u32 total) {
    Probable *p = (Probable*) Memory_alloc(TYPE_PROBABLE, sizeof(Probable));
    if (!p) return nullptr;
    Probable_setObject(p, object);
    Probable_setWeight(p, weight);
    Probable_setTotal(p, total);
    return p;
}

Probable *Probable_2(const Probable *init, bytes count) {
    if (count == 0) return nullptr;
    Probable *p = (Probable*) Memory_alloc(TYPE_PROBABLE_ARRAY, sizeof(Probable) * count);
    if (!p) return nullptr;
    
    if (init) {
        for (bytes i = 0; i < count; i++) {
            p[i] = *init;
        }
    } else {
        memset(p, 0, sizeof(Probable) * count);
    }
    return p;
}

fn Probable_free(Probable *p) {
    Memory_free(p);
}

pointer Probable_object(Probable *p) {
    if (!p) return 0;
    return Probable_object(p);
}

u32 Probable_weight(Probable *p) {
    if (!p) return 0;
    return Probable_weight(p);
}

u32 Probable_total(Probable *p) {
    if (!p) return 0;
    return Probable_total(p);
}

fn Probable_setObject(Probable *p, pointer object) {
    if (!p) return;
    Probable_setObject(p, object);
}

fn Probable_setWeight(Probable *p, u32 weight) {
    if (!p) return;
    Probable_setWeight(p, weight);
}

fn Probable_setTotal(Probable *p, u32 total) {
    if (!p) return;
    Probable_setTotal(p, total);
}

pointer Probable_get(Probable *p) {
    return Random_sample(Random_system(), p);
}
