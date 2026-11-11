#ifndef PRIMITIVE_INT_H
#define PRIMITIVE_INT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/int.h — Int primitive (Legacy: primitive/Int.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_intPool;

bool Int_init(void);
void Int_shutdown(void);
void *Int_alloc(void);
void *Int_allocArray(bytes count);
void Int_free(void *ptr);
i32 Int_get(void *ptr);
void Int_set(void *ptr, i32 value);
bool Int_compareAndSet(void *ptr, i32 expected, i32 value);
u64 Int_type(void *ptr);
bytes Int_length(void *ptr);


// Ergonomic constructors — Int() and Int(value)
void *Int_allocWithValue(i32 value);
#define Int_0(...) Int_alloc()
#define Int_1(value) Int_allocWithValue(value)
#define Int(...) CONSTRUCTOR_DISPATCH(Int, __VA_ARGS__)


#define Int_array(count) Int_allocArray(count)

#endif
