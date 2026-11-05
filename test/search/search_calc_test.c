#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "search/calc.h"
#include "search/find.h"
#include "search/spotlight.h"
#include "math/strict_math.h"

static int g_failures = 0;

#define CHECK(name, cond) do { \
    if (cond) { printf("[search_calc_test] PASS %s\n", name); } \
    else { printf("[search_calc_test] FAIL %s\n", name); g_failures++; } \
} while (0)

// Checks calculator parsing, search modes/ranking, and expression detection.
int main(void) {
    printf("=== Running String Calc & Search Subsystem Test Suite ===\n");

    // 1. String Math Expression Calculator (Exact user expression!)
    double userVal = 0.0;
    bool okUser = Calc_eval("sin(rad(90)) * atan(34) * pi", &userVal);
    CHECK("evaluate 'sin(rad(90)) * atan(34) * pi'", okUser);
    double expectedUser = sin(90.0 * STRICT_MATH_DEG_TO_RAD) * atan(34.0) * STRICT_MATH_PI;
    CHECK("user expression value is exact", fabs(userVal - expectedUser) < 1e-6);

    double val = 0.0;
    CHECK("precedence '2 + 3 * 4' = 14", Calc_eval("2 + 3 * 4", &val) && val == 14.0);
    CHECK("parentheses '(2 + 3) * 4' = 20", Calc_eval("(2 + 3) * 4", &val) && val == 20.0);
    CHECK("power '2 ^ 3 + 1' = 9", Calc_eval("2 ^ 3 + 1", &val) && val == 9.0);
    CHECK("trig 'cos(rad(0)) + sin(rad(90))' = 2", Calc_eval("cos(rad(0)) + sin(rad(90))", &val) && fabs(val - 2.0) < 1e-6);
    CHECK("functions 'sqrt(16) * abs(-5)' = 20", Calc_eval("sqrt(16) * abs(-5)", &val) && val == 20.0);
    CHECK("constants 'pi * 2' = tau", Calc_eval("pi * 2", &val) && fabs(val - STRICT_MATH_TWO_PI) < 1e-6);

    // Negative tests for calculator
    double dummy = 0.0;
    CHECK("invalid syntax fails closed", !Calc_eval("2 + * 4", &dummy));
    CHECK("empty string fails closed", !Calc_eval("", &dummy));
    CHECK("unclosed paren fails closed", !Calc_eval("(2 + 3", &dummy));

    // 2. IDE Find Flags & SQL LIKE Wildcards
    CHECK("SQL LIKE prefix 'Hello%' matches 'Hello World'", Search_like("Hello World", "Hello%", false));
    CHECK("SQL LIKE suffix '%World' matches 'Hello World'", Search_like("Hello World", "%World", false));
    CHECK("SQL LIKE middle '%lo%or%' matches 'Hello World'", Search_like("Hello World", "%lo%or%", false));
    CHECK("SQL LIKE single char 'a_c' matches 'abc'", Search_like("abc", "a_c", false));
    CHECK("SQL LIKE single char 'a_d' rejects 'abc'", !Search_like("abc", "a_d", false));
    CHECK("SQL LIKE case-insensitive default", Search_like("VexSpoke", "vex%", false));
    CHECK("SQL LIKE case-sensitive match", Search_like("VexSpoke", "Vex%", true));
    CHECK("SQL LIKE case-sensitive mismatch", !Search_like("VexSpoke", "vex%", true));

    // IDE Find Flags
    CHECK("FIND_EXACT_WORD matches isolated word",
          Search_match("player health is low", "health", FIND_EXACT_WORD));
    CHECK("FIND_EXACT_WORD rejects substring in word",
          !Search_match("player healthcare is low", "health", FIND_EXACT_WORD));
    CHECK("FIND_EXACT_WORD matches word at boundary",
          Search_match("health", "health", FIND_EXACT_WORD));

    // 3. Spotlight Ranking Engine
    const char *candidates[] = {
        "health",
        "health_bar",
        "player_health",
        "heavy_armor_stealth"
    };
    uint32_t ids[] = {10, 20, 30, 40};
    SpotlightMatch matches[4];
    size_t count = Spotlight_rank("health", candidates, ids, 4, matches, 4);
    CHECK("spotlight found 4 matches", count == 4);
    CHECK("rank #1 is exact match 'health'", matches[0].id == 10 && matches[0].score == 1000);
    CHECK("rank #2 is prefix match 'health_bar'", matches[1].id == 20);

    // Spotlight calculation detection
    double spotlightCalc = 0.0;
    CHECK("spotlight evaluates calculation query",
          Spotlight_tryCalculate("sin(rad(90)) * atan(34) * pi", &spotlightCalc) && fabs(spotlightCalc - expectedUser) < 1e-6);

    printf("\n=== String Calc & Search Subsystem Test Summary: %d failures ===\n", g_failures);
    return g_failures > 0 ? 1 : 0;
}
