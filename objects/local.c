#include "vex.h"
#include "re.h"
#include "objects/local.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Local
 * ============================================================================
 * Thread-local variable slot table object wrapper. Values live in an
 * arena-backed table indexed by thread id; the table starts at 8 slots and
 * f64s on demand (the Dynamic Scalability & Anti-Hardcoding Law), so the
 * thread-id space is never capped. Reads outside the live table return 0;
 * writes grow the table first. Local_2 deep-copies each element's table so
 * array instances own independent slot storage.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Local (objects/local.c)
 * ============================================================================
 * Thread-local variable slot table object wrapper. Values live in an
 * arena-backed table indexed by thread id; the table starts at 8 slots and
 * f64s on demand (the Dynamic Scalability & Anti-Hardcoding Law), so the
 * thread-id space is never capped. Reads outside the live table return 0;
 * writes grow the table first.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   Local {
 *     u64 *slots;   // arena-backed value table, indexed by threadId
 *     bytes slotCap;    // live table size (grows by doubling)
 *   }
 *
 * PRIVATE HELPERS (kept file-local pure-data only, each with full fields):
 * ----------------------------------------------------------------------------
 *   local_grow(self, threadId)   // d64 slotCap until threadId fits,
 *                                // zero new tail, free old table
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Local_0()
 *   - Local_2(init, count)
 *
 * Core Functions:
 *   - Local_free(local)
 *
 * Setters:
 *   - Local_set(local, threadId, value)
 *
 * Getters:
 *   - Local_get(local, threadId)
 * ============================================================================
 */


// local.c — Thread-local variable slot table object wrapper implementation.

typedef struct Local {
    u64 *slots;   // arena-backed value table, indexed by threadId
    bytes slotCap;    // live table size (grows by doubling)
} Local;

;;GETTER
static inline bytes Local_slotCap(const Local *s) { return (*s).slotCap; }
;;GETTER
static inline bytes *Local_slotCapMut(Local *s) { return &(*s).slotCap; }
;;SETTER
static inline fn Local_setSlotCap(Local *s, bytes v) { (*s).slotCap = v; }
;;GETTER
static inline u64 * Local_slots(const Local *s) { return (*s).slots; }
;;GETTER
static inline u64 * *Local_slotsMut(Local *s) { return &(*s).slots; }
;;SETTER
static inline fn Local_setSlots(Local *s, u64 * v) { (*s).slots = v; }


// Grow the slot table until threadId fits; the new tail is zeroed. On OOM
// the table is untouched and false is returned (the Cold-Strict,
// Hot-Minimal Validation Law drop-degrade).
static bool local_grow(Local *self, u32 threadId) {
    bytes need = (bytes) threadId + 1;
    bytes newCap = Local_slotCap(self) ? Local_slotCap(self) : 8;
    while (newCap < need) {
        if (newCap > SIZE_MAX / 2)
            return false;
        newCap *= 2;
    }
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_LOCAL) | MOD_LOCALE;
    u64 *nb = (u64*) Memory_alloc(type, newCap * sizeof(u64));
    if (!nb)
        return false;
    if (Local_slots(self)) {
        memcpy(nb, Local_slots(self), Local_slotCap(self) * sizeof(u64));
        Memory_free(Local_slots(self));
    }
    memset(nb + Local_slotCap(self), 0, (newCap - Local_slotCap(self)) * sizeof(u64));
    Local_setSlots(self, nb);
    Local_setSlotCap(self, newCap);
    return true;
}

Local *Local_0() {
    u64 type = Type_make(PROJ_VEXSPOKE, FORM_SINGLETON, ID_LOCAL) | MOD_LOCALE;
    Local *local = (Local*) Memory_alloc(type, sizeof(Local));
    if (!local)
        return nullptr;
    Local_setSlots(local, nullptr);
    Local_setSlotCap(local, 0);
    return local;
}


Local *Local_2(const Local *init, bytes count) {
    if (count == 0) return nullptr;
    Local *p = (Local*) Memory_alloc(TYPE_LOCAL_ARRAY, sizeof(Local) * count);
    if (!p) return nullptr;
    for (bytes i = 0; i < count; i++) {
        if (init) {
            // Deep copy: each element owns a table of init's live size.
            p[i].slots = nullptr;
            p[i].slotCap = 0;
            if (Local_slotCap(init)) {
                if (!local_grow(&p[i], (u32) (Local_slotCap(init) - 1))) {
                    for (bytes f = 0; f < i; f++) Memory_free(p[f].slots);
                    Memory_free(p);
                    return nullptr;
                }
                memcpy(p[i].slots, Local_slots(init), Local_slotCap(init) * sizeof(u64));
            }
        } else {
            p[i].slots = nullptr;
            p[i].slotCap = 0;
        }
    }
    return p;
}
fn Local_free(Local *local) {
    if (!local) return;
    if (Local_slots(local))
        Memory_free(Local_slots(local));
    Memory_free(local);
}

u64 Local_get(const Local *local, u32 threadId) {
    if (!local || !Local_slots(local) || threadId >= Local_slotCap(local))
        return 0;
    return Local_slots(local)[threadId];
}

fn Local_set(Local *local, u32 threadId, u64 value) {
    if (!local)
        return;
    if (threadId >= Local_slotCap(local)) {
        if (!local_grow(local, threadId))
            return;
    }
    Local_slots(local)[threadId] = value;
}
