#ifndef PRIMITIVE_BRAIN_H
#define PRIMITIVE_BRAIN_H

#include "vex.h"
// primitive/brain.h — Brain primitive (Legacy: primitive/Brain.java).
// Delegates to Bit16 width pool (2B stride).

extern BitPool g_brainPool;

bool Brain_init(void);
void Brain_shutdown(void);
any Brain_alloc(void);
any Brain_allocArray(bytes count);
void Brain_free(any ptr);
u16 Brain_get(any ptr);
void Brain_set(any ptr, u16 value);
bool Brain_compareAndSet(any ptr, u16 expected, u16 value);
u64 Brain_type(any ptr);
bytes Brain_length(any ptr);

// bf3216 — Brain IS bf16, helpers convert f32 <=> bf16 bits
u16 Brain_f32ToBFloat16(f32 value);
f32 Brain_bFloat16ToFloat(u16 bits);
f32 Brain_getFloat(any ptr);
void Brain_setFloat(any ptr, f32 value);


// Ergonomic constructors — Brain() and Brain(value)
any Brain_allocWithValue(u16 value);
#define Brain_0(...) Brain_alloc()
#define Brain_1(value) Brain_allocWithValue(value)
#define Brain(...) CONSTRUCTOR_DISPATCH(Brain, __VA_ARGS__)


#define Brain_array(count) Brain_allocArray(count)

#endif
