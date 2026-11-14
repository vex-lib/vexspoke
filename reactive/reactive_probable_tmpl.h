// reactive/reactive_probable_tmpl.h — the ReactiveProbable DECLARATION template.
//
// TEMPLATE, NOT A HEADER: no include guard — included once per instantiation.
// The caller defines VEX_NAME (the class) and VEX_T (the value type) first.
//
// A ReactiveProbable is a MERGED observable probable: { value, chance }. There is
// no nesting (Reactive(Probable(T)) does not exist) — the reactivity lives inside
// the object, as the engine contract requires. `chance` is in [0, 1]: get() yields
// the value with that probability, else the empty value (nullptr for a pointer, 0
// for a number).


#include "vex.h"

typedef struct VEX_NAME {
    Reactive base;   // the engine: word = the value, plus its observers
    f32    chance; // [0, 1] — the probability get() yields the value
} VEX_NAME;

VEX_NAME *VEX_CAT(VEX_NAME, _0)();
VEX_NAME *VEX_CAT(VEX_NAME, _2)(VEX_T value, f32 chance);
fn VEX_CAT(VEX_NAME, _free)(VEX_NAME *self);
fn VEX_CAT(VEX_NAME, _set)(VEX_NAME *self, VEX_T value, f32 chance);
VEX_T VEX_CAT(VEX_NAME, _get)(const VEX_NAME *self);
f32 VEX_CAT(VEX_NAME, _getChance)(const VEX_NAME *self);

VEX_CHANNELS_DECL(VEX_NAME, VEX_T);
