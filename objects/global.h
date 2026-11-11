#ifndef OBJECTS_GLOBAL_H
#define OBJECTS_GLOBAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "lang/constructor.h"

// objects/global.h — Atomic global pointer/variable object wrapper.
// Ported from legacy objects/Global.java.

typedef struct Global Global;

// Allocate a new atomic Global pointer/value
Global *Global_1(u64 initialValue);

// Free global memory

Global *Global_2(const Global *init, bytes count);

#define Global(...) CONSTRUCTOR_DISPATCH(Global, __VA_ARGS__)

void Global_free(Global *global);

// Atomic read/write
u64 Global_get(const Global *global);
void     Global_set(Global *global, u64 value);
bool     Global_compareAndSet(Global *global, u64 expected, u64 value);

#endif
