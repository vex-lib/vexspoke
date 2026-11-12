#include "vex.h"
#include "search/calc.h"
#include "types.h"
#include "math/strict_math.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Calc
 * ============================================================================
 * Allocation-free recursive-descent math expression parser: evaluates strings
 * like "sin(rad(90)) * atan(34) * pi" into d64/f32 scalars with full
 * operator precedence (+, -, *, /, %, ^), unary minus, constants (pi, e,
 * tau), and scientific/trigonometric functions. The CalcParser cursor struct
 * lives on the stack — zero heap traffic on the eval path; syntax errors
 * fail closed with false and a fallback value. Lives at R2 as a leaf search
 * behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Calc (search/calc.c — defined in search/calc.h)
 * ============================================================================
 * Fast recursive-descent math parser supporting operator precedence, constants,
 * and scientific / trigonometric functions. Evaluates in-place on the stack.
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   CalcParser {
 *     const char *p;  // current parse cursor into the expression string
 *     bool hasError;  // syntax-error latch, fails the eval closed
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - Calc_eval(expr, outValue)
 *   - Calc_evalFloat(expr, outValue)
 *   - Calc_evalWithFallback(expr, fallback)
 * Private Core Functions: (.c static)
 *   - skip_whitespace(cp)
 *   - parse_primary(cp)
 *   - parse_factor(cp)
 *   - parse_term(cp)
 *   - parse_expression(cp)
 * ============================================================================
 */

typedef struct CalcParser {
    const char *p;
    bool hasError;
} CalcParser;

// Advances the parser cursor past ASCII whitespace.
static void skip_whitespace(CalcParser *cp) {
    while (*(*cp).p != '\0' && isspace((unsigned char)*(*cp).p)) {
        (*cp).p++;
    }
}

static d64 parse_expression(CalcParser *cp);

// Parses a number, parenthesized expression, constant, or supported function call.
static d64 parse_primary(CalcParser *cp) {
    skip_whitespace(cp);
    if (*(*cp).p == '\0') {
        (*cp).hasError = true;
        return 0.0;
    }

    // Unary plus and minus
    if (*(*cp).p == '+') {
        (*cp).p++;
        return parse_primary(cp);
    }
    if (*(*cp).p == '-') {
        (*cp).p++;
        return -parse_primary(cp);
    }

    // Parenthesized sub-expression
    if (*(*cp).p == '(') {
        (*cp).p++;
        d64 val = parse_expression(cp);
        skip_whitespace(cp);
        if (*(*cp).p == ')') {
            (*cp).p++;
        } else {
            (*cp).hasError = true;
        }
        return val;
    }

    // Number literal
    if (isdigit((unsigned char)*(*cp).p) || *(*cp).p == '.') {
        char *endptr = nullptr;
        d64 val = strtod((*cp).p, &endptr);
        if (endptr == (*cp).p) {
            (*cp).hasError = true;
            return 0.0;
        }
        (*cp).p = endptr;
        return val;
    }

    // Identifier: constant or function
    if (isalpha((unsigned char)*(*cp).p) || *(*cp).p == '_') {
        char name[32];
        bytes n = 0;
        while ((isalnum((unsigned char)*(*cp).p) || *(*cp).p == '_') && n < sizeof(name) - 1) {
            name[n++] = (char) tolower((unsigned char)*(*cp).p);
            (*cp).p++;
        }
        name[n] = '\0';
        skip_whitespace(cp);

        // Check constants
        if (strcmp(name, "pi") == 0) return STRICT_MATH_PI;
        if (strcmp(name, "e") == 0) return STRICT_MATH_E;
        if (strcmp(name, "tau") == 0) return STRICT_MATH_TAU;

        // Function call: expect '('
        if (*(*cp).p != '(') {
            (*cp).hasError = true;
            return 0.0;
        }
        (*cp).p++; // consume '('

        // Parse arguments
        d64 arg1 = parse_expression(cp);
        d64 arg2 = 0.0;
        d64 arg3 = 0.0;
        skip_whitespace(cp);
        if (*(*cp).p == ',') {
            (*cp).p++;
            arg2 = parse_expression(cp);
            skip_whitespace(cp);
            if (*(*cp).p == ',') {
                (*cp).p++;
                arg3 = parse_expression(cp);
                skip_whitespace(cp);
            }
        }
        if (*(*cp).p == ')') {
            (*cp).p++;
        } else {
            (*cp).hasError = true;
            return 0.0;
        }

        // Evaluate function
        if (strcmp(name, "sin") == 0) return sin(arg1);
        if (strcmp(name, "cos") == 0) return cos(arg1);
        if (strcmp(name, "tan") == 0) return tan(arg1);
        if (strcmp(name, "asin") == 0) return asin(arg1);
        if (strcmp(name, "acos") == 0) return acos(arg1);
        if (strcmp(name, "atan") == 0) return atan(arg1);
        if (strcmp(name, "atan2") == 0) return atan2(arg1, arg2);
        if (strcmp(name, "rad") == 0) return arg1 * STRICT_MATH_DEG_TO_RAD;
        if (strcmp(name, "deg") == 0) return arg1 * STRICT_MATH_RAD_TO_DEG;
        if (strcmp(name, "sqrt") == 0) return arg1 >= 0.0 ? sqrt(arg1) : 0.0;
        if (strcmp(name, "invsqrt") == 0) return arg1 > 0.0 ? 1.0 / sqrt(arg1) : 0.0;
        if (strcmp(name, "abs") == 0) return fabs(arg1);
        if (strcmp(name, "round") == 0) return round(arg1);
        if (strcmp(name, "floor") == 0) return floor(arg1);
        if (strcmp(name, "ceil") == 0) return ceil(arg1);
        if (strcmp(name, "min") == 0) return arg1 < arg2 ? arg1 : arg2;
        if (strcmp(name, "max") == 0) return arg1 > arg2 ? arg1 : arg2;
        if (strcmp(name, "clamp") == 0) return StrictMath_clampD(arg1, arg2, arg3);
        if (strcmp(name, "pow") == 0) return pow(arg1, arg2);
        if (strcmp(name, "exp") == 0) return exp(arg1);
        if (strcmp(name, "log") == 0) return arg1 > 0.0 ? log(arg1) : 0.0;

        (*cp).hasError = true;
        return 0.0;
    }

    (*cp).hasError = true;
    return 0.0;
}

// Parses exponentiation with right-associative precedence.
static d64 parse_factor(CalcParser *cp) {
    d64 base = parse_primary(cp);
    skip_whitespace(cp);
    while (*(*cp).p == '^') {
        (*cp).p++;
        d64 expVal = parse_factor(cp); // right-associative
        base = pow(base, expVal);
        skip_whitespace(cp);
    }
    return base;
}

// Parses multiplication, division, and remainder above additive precedence.
static d64 parse_term(CalcParser *cp) {
    d64 left = parse_factor(cp);
    skip_whitespace(cp);
    while (*(*cp).p == '*' || *(*cp).p == '/' || *(*cp).p == '%') {
        char op = *(*cp).p++;
        d64 right = parse_factor(cp);
        if (op == '*') {
            left *= right;
        } else if (op == '/') {
            if (right == 0.0) {
                (*cp).hasError = true;
                return 0.0;
            }
            left /= right;
        } else if (op == '%') {
            if (right == 0.0) {
                (*cp).hasError = true;
                return 0.0;
            }
            left = fmod(left, right);
        }
        skip_whitespace(cp);
    }
    return left;
}

// Parses left-associative addition and subtraction expressions.
static d64 parse_expression(CalcParser *cp) {
    d64 left = parse_term(cp);
    skip_whitespace(cp);
    while (*(*cp).p == '+' || *(*cp).p == '-') {
        char op = *(*cp).p++;
        d64 right = parse_term(cp);
        if (op == '+') {
            left += right;
        } else {
            left -= right;
        }
        skip_whitespace(cp);
    }
    return left;
}

// Evaluates a complete expression into outValue; leaves output unchanged on failure.
bool Calc_eval(const char *expr, d64 *outValue) {
    if (!expr || !outValue) return false;
    CalcParser cp = { .p = expr, .hasError = false };
    skip_whitespace(&cp);
    if (*cp.p == '\0') return false;

    d64 res = parse_expression(&cp);
    skip_whitespace(&cp);
    if (cp.hasError || *cp.p != '\0') {
        return false;
    }
    *outValue = res;
    return true;
}

// Evaluates an expression and converts its result to f32 on success.
bool Calc_evalFloat(const char *expr, f32 *outValue) {
    if (!outValue) return false;
    d64 d = 0.0;
    if (!Calc_eval(expr, &d)) return false;
    *outValue = (f32) d;
    return true;
}

// Evaluates an expression, returning fallback when parsing or evaluation fails.
d64 Calc_evalWithFallback(const char *expr, d64 fallback) {
    d64 d = 0.0;
    if (Calc_eval(expr, &d)) return d;
    return fallback;
}
