#ifndef PRIMITIVE_BYTE_H
#define PRIMITIVE_BYTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "re/nio/bit_pool.h"

#include "lang/constructor.h"

// primitive/byte.h — Byte primitive (Legacy: primitive/Byte.java).
// Delegates to Bit8 width pool (1B stride).

extern BitPool g_bytePool;

bool Byte_init(void);
void Byte_shutdown(void);
void *Byte_alloc(void);
void *Byte_allocArray(bytes count);
void Byte_free(void *ptr);
 Byte_get(void *ptr);
void Byte_set(void *ptr,  value);
bool Byte_compareAndSet(void *ptr,  expected,  value);
u64 Byte_type(void *ptr);
bytes Byte_length(void *ptr);


// Ergonomic constructors — Byte() and Byte(value)
void *Byte_allocWithValue( value);
#define Byte_0(...) Byte_alloc()
#define Byte_1(value) Byte_allocWithValue(value)
#define Byte(...) CONSTRUCTOR_DISPATCH(Byte, __VA_ARGS__)


#define Byte_array(count) Byte_allocArray(count)

#endif
