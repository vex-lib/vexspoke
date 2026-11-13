#ifndef PRIMITIVE_LONG_H
#define PRIMITIVE_LONG_H

#include "vex.h"
// primitive/long.h — Long primitive (Legacy: primitive/Long.java).
// Delegates to Bit64 width pool (8B stride).

extern BitPool g_longPool;

bool Long_init(void);
void Long_shutdown(void);
any Long_alloc(void);
any Long_allocArray(bytes count);
void Long_free(any ptr);
int64_t Long_get(any ptr);
void Long_set(any ptr, int64_t value);
bool Long_compareAndSet(any ptr, int64_t expected, int64_t value);
u64 Long_type(any ptr);
bytes Long_length(any ptr);


// Ergonomic constructors — Long() and Long(value)
any Long_allocWithValue(int64_t value);
#define Long_0(...) Long_alloc()
#define Long_1(value) Long_allocWithValue(value)
#define Long(...) CONSTRUCTOR_DISPATCH(Long, __VA_ARGS__)


#define Long_array(count) Long_allocArray(count)

#endif
