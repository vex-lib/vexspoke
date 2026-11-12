#ifndef PRIMITIVE_BOOL_H
#define PRIMITIVE_BOOL_H

#include "vex.h"
// primitive/bool.h — Bool primitive (Legacy: primitive/Bool.java).
// Delegates to Bit8 width pool (1B stride).

extern BitPool g_boolPool;

bool Bool_init(void);
void Bool_shutdown(void);
void *Bool_alloc(void);
void *Bool_allocArray(bytes count);
void Bool_free(void *ptr);
bool Bool_get(void *ptr);
void Bool_set(void *ptr, bool value);
bool Bool_compareAndSet(void *ptr, bool expected, bool value);
u64 Bool_type(void *ptr);
bytes Bool_length(void *ptr);


// Ergonomic constructors — Bool() and Bool(value)
void *Bool_allocWithValue(bool value);
#define Bool_0(...) Bool_alloc()
#define Bool_1(value) Bool_allocWithValue(value)
#define Bool(...) CONSTRUCTOR_DISPATCH(Bool, __VA_ARGS__)


#define Bool_array(count) Bool_allocArray(count)

#endif
