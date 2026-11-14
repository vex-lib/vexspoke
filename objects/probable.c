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


Probable *Probable_3(pointer object, u32 weight, u32 total) {
    Probable *p = (Probable*) Memory_alloc(TYPE_PROBABLE, sizeof(Probable));
    if (!p) return nullptr;
    (*p).object = object;
    (*p).weight = weight;
    (*p).total = total;
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
    return (*p).object;
}

u32 Probable_weight(Probable *p) {
    if (!p) return 0;
    return (*p).weight;
}

u32 Probable_total(Probable *p) {
    if (!p) return 0;
    return (*p).total;
}

fn Probable_setObject(Probable *p, pointer object) {
    if (!p) return;
    (*p).object = object;
}

fn Probable_setWeight(Probable *p, u32 weight) {
    if (!p) return;
    (*p).weight = weight;
}

fn Probable_setTotal(Probable *p, u32 total) {
    if (!p) return;
    (*p).total = total;
}

pointer Probable_get(Probable *p) {
    return Random_sample(Random_system(), p);
}
