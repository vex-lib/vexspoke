#ifndef PRIMITIVE_FLOAT_H
#define PRIMITIVE_FLOAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/f32.h — Float primitive (Legacy: primitive/Float.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_f32Pool;

bool Float_init(void);
void Float_shutdown(void);
void *Float_alloc(void);
void *Float_allocArray(bytes count);
void Float_free(void *ptr);
f32 Float_get(void *ptr);
void Float_set(void *ptr, f32 value);
bool Float_compareAndSet(void *ptr, f32 expected, f32 value);
u64 Float_type(void *ptr);
bytes Float_length(void *ptr);


// Ergonomic constructors — Float() and Float(value)
void *Float_allocWithValue(f32 value);
#define Float_0(...) Float_alloc()
#define Float_1(value) Float_allocWithValue(value)
#define Float(...) CONSTRUCTOR_DISPATCH(Float, __VA_ARGS__)


#define Float_array(count) Float_allocArray(count)

#endif
