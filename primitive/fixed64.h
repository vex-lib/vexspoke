#ifndef PRIMITIVE_FIXED64_H
#define PRIMITIVE_FIXED64_H

#include "vex.h"

// primitive/fixed64.h — Fixed64 primitive (Legacy: primitive/Fixed64.java).
// Delegates to Bit64 width pool (8B stride).

extern BitPool g_fixed64Pool;

bool Fixed64_init();
fn Fixed64_shutdown();
any Fixed64_alloc();
any Fixed64_allocArray(bytes count);
fn Fixed64_free(any ptr);
int64_t Fixed64_get(any ptr);
fn Fixed64_set(any ptr, int64_t value);
bool Fixed64_compareAndSet(any ptr, int64_t expected, int64_t value);
u64 Fixed64_type(any ptr);
bytes Fixed64_length(any ptr);


// Ergonomic constructors — Fixed64() and Fixed64(value)
any Fixed64_allocWithValue(int64_t value);
#define Fixed64_0(...) Fixed64_alloc()
#define Fixed64_1(value) Fixed64_allocWithValue(value)
#define Fixed64(...) CONSTRUCTOR_DISPATCH(Fixed64, __VA_ARGS__)


#define Fixed64_array(count) Fixed64_allocArray(count)

#endif
