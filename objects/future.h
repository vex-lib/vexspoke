#ifndef OBJECTS_FUTURE_H
#define OBJECTS_FUTURE_H

#include "vex.h"

// objects/future.h — Asynchronous single-assignment Future object wrapper.
// Ported from legacy objects/Future.java.

typedef struct Future Future;

// Allocate a new unresolved Future instance
Future *Future_0();

// Free future memory

Future *Future_2(const Future *init, bytes count);

#define Future(...) CONSTRUCTOR_DISPATCH(Future, __VA_ARGS__)

fn Future_free(Future *future);

// Query whether the future value has been fulfilled
bool Future_isGiven(const Future *future);

// Get the resolved value (returns 0 if not yet given)
u64 Future_get(const Future *future);

// Atomically fulfill the future with a value. Returns true on success, false if already fulfilled.
bool Future_setDesiredValue(Future *future, u64 value);

#endif
