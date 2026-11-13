#ifndef GENERIC_GENERIC_CAT_H
#define GENERIC_GENERIC_CAT_H

#include "vex.h"

// generic/generic_cat.h — the token paste every generic stamp is built on.
//
// VEX_CAT is the two-level paste: a `##` operand is never expanded, so the
// outer macro — which is NOT a paste context — expands its argument first,
// then the inner paste joins the results (GCC Argument Prescan; ISO C23
// 6.10.3.1). Without the two levels, List(VEX_ALIAS) would paste the alias
// name instead of what it names.
//
// Guarded because reactive/generic.h owns the same two lines: an identical
// macro redefinition is legal C, but only one spelling should reach the
// compiler, and the first header included wins.

#ifndef VEX_CAT
#define VEX_CAT_(a, b) a##b
#define VEX_CAT(a, b)  VEX_CAT_(a, b)
#endif

// VEX_STR is the matching two-level stringize: `#` suppresses expansion of its
// operand, so the outer macro expands the argument first and the inner `#`
// spells the result. The generic stamps use it to write the class name
// annotation from the element token, including a nested `List_<T>`.
#ifndef VEX_STR
#define VEX_STR_(x) #x
#define VEX_STR(x)  VEX_STR_(x)
#endif

#endif // GENERIC_GENERIC_CAT_H
