#ifndef PRIMITIVE_BOOL_H
#define PRIMITIVE_BOOL_H

#include "vex.h"

// primitive/bool.h — Bool primitive (Legacy: primitive/Bool.java).
// Delegates to Bit8 width pool (1B stride).

extern BitPool g_boolPool;

bool Bool_init();
fn Bool_shutdown();
any Bool_alloc();
any Bool_allocArray(bytes count);
fn Bool_free(any ptr);
bool Bool_get(any ptr);
fn Bool_set(any ptr, bool value);
bool Bool_compareAndSet(any ptr, bool expected, bool value);
u64 Bool_type(any ptr);
bytes Bool_length(any ptr);


// Ergonomic constructors — Bool() and Bool(value)
any Bool_allocWithValue(bool value);
#define Bool_0(...) Bool_alloc()
#define Bool_1(value) Bool_allocWithValue(value)
#define Bool(...) CONSTRUCTOR_DISPATCH(Bool, __VA_ARGS__)


#define Bool_array(count) Bool_allocArray(count)

#endif
