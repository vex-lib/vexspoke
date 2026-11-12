#ifndef C23_C23_H
#define C23_C23_H

// c23/c23.h — THE c23 directory header.
// One include for the C23 relational helpers: equals, fn, type_overload, zero.
//
// NOTE: c23/free.h is deliberately NOT included here. It redirects the
// standard `free` to c23_free and is opt-in per translation unit (see the
// note in c23/free.h and nio/mem.c). Include "c23/free.h" yourself where you
// want that behavior.
// Sub-headers stay includable individually; this header is the door engine
// code uses.

#include "vex.h"

#include "c23/equals.h"
#include "c23/fn.h"
#include "c23/type_overload.h"
#include "c23/zero.h"

#endif // C23_C23_H
