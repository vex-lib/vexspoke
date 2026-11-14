#ifndef PRIMITIVE_SHORT_H
#define PRIMITIVE_SHORT_H

#include "vex.h"

// primitive/short.h — Short primitive (Legacy: primitive/Short.java).
// Delegates to Bit16 width pool (2B stride).

extern BitPool g_shortPool;

bool Short_init();
fn Short_shutdown();
any Short_alloc();
any Short_allocArray(bytes count);
fn Short_free(any ptr);
i16 Short_get(any ptr);
fn Short_set(any ptr, i16 value);
bool Short_compareAndSet(any ptr, i16 expected, i16 value);
u64 Short_type(any ptr);
bytes Short_length(any ptr);


// Ergonomic constructors — Short() and Short(value)
any Short_allocWithValue(i16 value);
#define Short_0(...) Short_alloc()
#define Short_1(value) Short_allocWithValue(value)
#define Short(...) CONSTRUCTOR_DISPATCH(Short, __VA_ARGS__)


#define Short_array(count) Short_allocArray(count)

#endif
