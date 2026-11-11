#ifndef OBJECTS_PASSIVE_H
#define OBJECTS_PASSIVE_H

#include <stdint.h>
#include <stddef.h>
#include "lang/constructor.h"

// objects/passive.h — Lazy-evaluated / computed Passive object wrapper.
// Ported from legacy objects/Passive.java.

typedef u64 (*PassiveGetter)(void *userdata);
typedef void (*PassiveSetter)(u64 value, void *userdata);

typedef struct Passive Passive;

// Allocate a new Passive instance with lazy getter/setter hooks
Passive *Passive_3(PassiveGetter getter, PassiveSetter setter, void *userdata);

// Free passive memory

Passive *Passive_2(const Passive *init, bytes count);

#define Passive(...) CONSTRUCTOR_DISPATCH(Passive, __VA_ARGS__)

void Passive_free(Passive *passive);

// Accessors invoking lazy hooks
u64 Passive_get(Passive *passive);
void     Passive_set(Passive *passive, u64 value);

#endif
