#ifndef PRIMITIVE_SHORT_H
#define PRIMITIVE_SHORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/short.h — Short primitive (Legacy: primitive/Short.java).
// Delegates to Bit16 width pool (2B stride).

extern BitPool g_shortPool;

bool Short_init(void);
void Short_shutdown(void);
void *Short_alloc(void);
void *Short_allocArray(bytes count);
void Short_free(void *ptr);
 Short_get(void *ptr);
void Short_set(void *ptr,  value);
bool Short_compareAndSet(void *ptr,  expected,  value);
u64 Short_type(void *ptr);
bytes Short_length(void *ptr);


// Ergonomic constructors — Short() and Short(value)
void *Short_allocWithValue( value);
#define Short_0(...) Short_alloc()
#define Short_1(value) Short_allocWithValue(value)
#define Short(...) CONSTRUCTOR_DISPATCH(Short, __VA_ARGS__)


#define Short_array(count) Short_allocArray(count)

#endif
