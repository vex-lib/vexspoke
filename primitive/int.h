#ifndef PRIMITIVE_INT_H
#define PRIMITIVE_INT_H

#include "vex.h"
// primitive/int.h — Int primitive (Legacy: primitive/Int.java).
// Delegates to Bit32 width pool (4B stride).

extern BitPool g_intPool;

bool Int_init(void);
void Int_shutdown(void);
any Int_alloc(void);
any Int_allocArray(bytes count);
void Int_free(any ptr);
i32 Int_get(any ptr);
void Int_set(any ptr, i32 value);
bool Int_compareAndSet(any ptr, i32 expected, i32 value);
u64 Int_type(any ptr);
bytes Int_length(any ptr);


// Ergonomic constructors — Int() and Int(value)
any Int_allocWithValue(i32 value);
#define Int_0(...) Int_alloc()
#define Int_1(value) Int_allocWithValue(value)
#define Int(...) CONSTRUCTOR_DISPATCH(Int, __VA_ARGS__)


#define Int_array(count) Int_allocArray(count)

#endif
