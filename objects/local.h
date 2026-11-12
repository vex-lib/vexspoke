#ifndef OBJECTS_LOCAL_H
#define OBJECTS_LOCAL_H

#include "vex.h"

// objects/local.h — Thread-local variable slot table object wrapper.
// Ported from legacy objects/Local.java. The slot table is arena-backed
// and grows exponentially on demand (the Dynamic Scalability &
// Anti-Hardcoding Law) — thread ids are never capped.

typedef struct Local Local;

// Allocate a new thread-local slot table
Local *Local_0(void);

// Free local memory

Local *Local_2(const Local *init, bytes count);

#define Local(...) CONSTRUCTOR_DISPATCH(Local, __VA_ARGS__)

void Local_free(Local *local);

// Slot accessors per thread id — reads outside the live table return 0,
// writes grow the table first.
u64 Local_get(const Local *local, u32 threadId);
void     Local_set(Local *local, u32 threadId, u64 value);

#endif
