#ifndef C23_EQUALS_H
#define C23_EQUALS_H

#include "vex.h"

// c23/equals.h — Relational Equality.
//
// Java `.equals()` compares content; C `==` compares addresses. These close the
// gap the relational way: identity first, then the block identity header (type +
// length must match), then the payload Bytes compared directly. Foreign
// (non-allocator) pointers can only prove identity.

bool isEqual(const any a, const any b);

// Registry form: resolve two symbol names in `v` and compare their payloads.
// False when either name is unregistered or their class ids differ.
struct SymbolTable;
bool isEquallyNamed(struct SymbolTable *v, const char *nameA, const char *nameB);

#endif
