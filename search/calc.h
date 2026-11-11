#ifndef SEARCH_CALC_H
#define SEARCH_CALC_H

#include "lang/primitive.h"

// search/calc.h — String Mathematical Expression Calculation Engine.
//
// Single Class Per File Law: Calc.
//
// Evaluates arbitrary math expressions from strings (e.g. "sin(rad(90)) * atan(34) * pi")
// directly into d64/f32 scalar values. Allocation-free Pratt precedence parser.
//
// Supported operators:
//   +, -, *, /, %, ^ (power), unary -
//
// Supported functions:
//   sin, cos, tan, asin, acos, atan, atan2, rad, deg, sqrt, invsqrt,
//   abs, round, floor, ceil, min, max, clamp, pow, exp, log
//
// Supported constants:
//   pi, e, tau

// Evaluates an expression string into a d64 result. Returns true on success, false on syntax error.
bool Calc_eval(const char *expr, d64 *outValue);

// Evaluates into a f32 result.
bool Calc_evalFloat(const char *expr, f32 *outValue);

// Convenience: returns value directly, or fallback upon error.
d64 Calc_evalWithFallback(const char *expr, d64 fallback);

#endif
