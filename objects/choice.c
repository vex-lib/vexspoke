#include "vex.h"
#include "re.h"
#include "objects/choice.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Choice
 * ============================================================================
 * Immutable deterministic choice / branch dispatcher: a fixed array of
 * ChoiceSlot records (object pointer payload plus per-option callback) in a
 * flexible-array struct, arena-allocated with the WRAP2_CHOICE type tag.
 * Choice_3 builds a single choice from parallel pointer/callback arrays;
 * Choice_2 builds an array of choices. trigger() invokes the indexed slot's
 * callback with its object payload and caller userdata; length/getObject
 * expose the option table read-only.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Choice (objects/choice.c)
 * ============================================================================
 * Immutable deterministic choice / branch dispatcher.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   ChoiceSlot {
 *     u64 objectPtr;        // option object pointer payload
 *     ChoiceCallback callback;   // per-option branch callback
 *   }
 *   Choice {
 *     bytes count;              // number of live options
 *     ChoiceSlot slots[];        // flexible option array
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Choice_3(objectPtrs, callbacks, count)
 *   - Choice_2(init, count)
 *
 * Core Functions:
 *   - Choice_free(choice)
 *   - Choice_length(choice)
 *   - Choice_trigger(choice, index, userdata)
 *
 * Getters:
 *   - Choice_getObject(choice, index)
 * ============================================================================
 */


// choice.c — Immutable deterministic choice / branch dispatcher implementation.

typedef struct ChoiceSlot {
    u64       objectPtr;
    ChoiceCallback callback;
} ChoiceSlot;

typedef struct Choice {
    bytes     count;
    ChoiceSlot slots[];
} Choice;

// Allocates one choice from parallel object and callback arrays.
Choice *Choice_3(const u64 *objectPtrs, const ChoiceCallback *callbacks, bytes count) {
    if (count == 0) return nullptr;
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_CHOICE) | WRAP2_CHOICE;
    bytes Bytes = sizeof(Choice) + count * sizeof(ChoiceSlot);
    Choice *choice = (Choice*) Memory_alloc(type, Bytes);
    if (!choice)
        return nullptr;
    (*choice).count = count;
    for (bytes i = 0; i < count; i++) {
        (*choice).slots[i].objectPtr = objectPtrs ? objectPtrs[i] : 0;
        (*choice).slots[i].callback = callbacks ? callbacks[i] : nullptr;
    }
    return choice;
}


// Allocates count choices, copying init when supplied or zeroing otherwise.
Choice *Choice_2(const Choice *init, bytes count) {
    if (count == 0) return nullptr;
    Choice *p = (Choice*) Memory_alloc(TYPE_CHOICE_ARRAY, sizeof(Choice) * count);
    if (!p) return nullptr;
    if (init) {
        for (bytes i = 0; i < count; i++) p[i] = *init;
    } else {
        memset(p, 0, sizeof(Choice) * count);
    }
    return p;
}
// Releases the choice allocation through the engine-owned Memory ABI.
void Choice_free(Choice *choice) {
    if (!choice) return;
    Memory_free(choice);
}

// Returns the number of options, or zero for a null choice.
bytes Choice_length(const Choice *choice) {
    return choice ? (*choice).count : 0;
}

// Returns the indexed object payload, or zero when choice or index is invalid.
u64 Choice_getObject(const Choice *choice, bytes index) {
    if (!choice || index >= (*choice).count)
        return 0;
    return (*choice).slots[index].objectPtr;
}

// Invokes the selected option callback with its object payload and userdata.
void Choice_trigger(const Choice *choice, bytes index, void *userdata) {
    if (!choice || index >= (*choice).count)
        return;
    ChoiceCallback cb = (*choice).slots[index].callback;
    if (cb) {
        cb((*choice).slots[index].objectPtr, userdata);
    }
}
