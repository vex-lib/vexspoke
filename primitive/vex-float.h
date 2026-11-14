#ifndef PRIMITIVE_FLOAT_H
#define PRIMITIVE_FLOAT_H

#include "vex.h"

// primitive/f32.h — Float primitive (Legacy: primitive/Float.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_f32Pool;

bool Float_init();
fn Float_shutdown();
any Float_alloc();
any Float_allocArray(bytes count);
fn Float_free(any ptr);
f32 Float_get(any ptr);
fn Float_set(any ptr, f32 value);
bool Float_compareAndSet(any ptr, f32 expected, f32 value);
u64 Float_type(any ptr);
bytes Float_length(any ptr);


// Ergonomic constructors — Float() and Float(value)
any Float_allocWithValue(f32 value);
#define Float_0(...) Float_alloc()
#define Float_1(value) Float_allocWithValue(value)
#define Float(...) CONSTRUCTOR_DISPATCH(Float, __VA_ARGS__)


#define Float_array(count) Float_allocArray(count)

#endif
