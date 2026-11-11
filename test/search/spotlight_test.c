// vexspoke/test/search/spotlight_test.c — owner test for search/spotlight.
//
// Proves the ranked search + calculator seam:
//   - ranking tiers: exact beats prefix beats word-boundary beats substring
//     beats fuzzy subsequence, and results are sorted descending;
//   - candidate ranks are case-insensitive, and non-matches score 0 (excluded);
//   - caller-supplied ids are echoed, otherwise the candidate index is used;
//   - the return value is capped at maxCount (RECORDED defect: the header
//     promises the total match count, but the implementation stops counting);
//   - nullptr / zero-capacity refusals;
//   - tryCalculate delegates to Calc_eval: a real expression computes, a
//     non-expression returns false, and nullptr is refused.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "search/spotlight.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

#define GAP(cond, msg)                                                     \
    do {                                                                   \
        if (!(cond))                                                       \
            printf("KNOWN GAP (unproved): %s\n", msg);                     \
    } while (0)

// Checks exact/prefix/subsequence ranking order and default candidate-index IDs.
static void test_exact_and_prefix(void) {
    const char *cands[] = { "app", "apple", "apple pie", "banana", "pineapple" };
    u32 ids[] = { 10, 20, 30, 40, 50 };
    SpotlightMatch out[8];

    bytes n = Spotlight_rank("apple", cands, ids, 5, out, 8);
    CHECK(n == 3);                              // app excluded (no match), banana excluded
    // Exact match "apple" ranks first.
    CHECK(out[0].score == 1000);
    CHECK(out[0].id == 20);
    // Prefix "apple pie" next; "pineapple" is a substring.
    CHECK(out[1].score < 1000 && out[1].score > out[2].score);
    CHECK(out[2].score > 0);
    // Descending order throughout.
    for (bytes i = 1; i < n; i++)
        CHECK(out[i - 1].score >= out[i].score);

    // ids==nullptr maps to candidate index.
    n = Spotlight_rank("apple", cands, nullptr, 5, out, 8);
    CHECK(n == 3);
    for (bytes i = 0; i < n; i++)
        CHECK(out[i].id < 5);
}

// Pins ordering across exact, prefix, word-boundary, substring, and fuzzy match tiers.
static void test_tiers(void) {
    // One candidate per tier for a single query.
    const char *cands[] = {
        "bar",          // exact
        "barista",      // prefix
        "foo_bar",      // word-boundary prefix
        "sandbar",      // substring
        "b_a_r",        // fuzzy subsequence only
        "zzz",          // no match
    };
    u32 ids[] = { 1, 2, 3, 4, 5, 6 };
    SpotlightMatch out[8];
    bytes n = Spotlight_rank("bar", cands, ids, 6, out, 8);
    CHECK(n == 5);
    CHECK(out[0].id == 1);                      // exact
    CHECK(out[1].id == 2);                      // prefix
    CHECK(out[2].id == 3);                      // word boundary
    CHECK(out[3].id == 4);                      // substring
    CHECK(out[4].id == 5);                      // fuzzy
    CHECK(out[0].score > out[1].score);
    CHECK(out[1].score > out[2].score);
    CHECK(out[2].score > out[3].score);
    CHECK(out[3].score > out[4].score);
}

// Checks candidate matching is case-insensitive for differently cased exact strings.
static void test_case_and_boundaries(void) {
    const char *cands[] = { "APPLE", "Apple", "apple" };
    SpotlightMatch out[4];
    bytes n = Spotlight_rank("apple", cands, nullptr, 3, out, 4);
    CHECK(n == 3);
    CHECK(out[0].score == 1000 && out[1].score == 1000 && out[2].score == 1000);
}

// Checks rank output capacity and the reported total count, including zero capacity.
static void test_max_count(void) {
    const char *cands[] = { "aa", "ab", "ac", "ad" };
    SpotlightMatch out[2];
    bytes n = Spotlight_rank("a", cands, nullptr, 4, out, 2);
    CHECK(n == 4);                              // documented TOTAL match count
    CHECK(out[0].score > 0 && out[1].score > 0); // only maxCount written

    // Zero maxCount refuses.
    CHECK(Spotlight_rank("a", cands, nullptr, 4, out, 0) == 0);
}

// Checks null and empty query inputs return no ranked matches.
static void test_nulls(void) {
    const char *cands[] = { "a" };
    SpotlightMatch out[1];
    CHECK(Spotlight_rank(nullptr, cands, nullptr, 1, out, 1) == 0);
    CHECK(Spotlight_rank("a", nullptr, nullptr, 1, out, 1) == 0);
    CHECK(Spotlight_rank("a", cands, nullptr, 1, nullptr, 1) == 0);
    CHECK(Spotlight_rank("", cands, nullptr, 1, out, 1) == 0);   // empty query
}

// Checks Spotlight calculation forwarding for valid, non-expression, and null inputs.
static void test_calculate(void) {
    d64 r = 0.0;
    CHECK(Spotlight_tryCalculate("2 + 3 * 4", &r));
    CHECK(r == 14.0);
    CHECK(Spotlight_tryCalculate("10 / 4", &r));
    CHECK(r == 2.5);
    CHECK(!Spotlight_tryCalculate("this is not math", &r));
    CHECK(!Spotlight_tryCalculate(nullptr, &r));
    CHECK(!Spotlight_tryCalculate("1+1", nullptr));
}

// Runs ranking tiers, candidate boundaries, capacity behavior, and calculation checks.
int main(void) {
    test_exact_and_prefix();
    test_tiers();
    test_case_and_boundaries();
    test_max_count();
    test_nulls();
    test_calculate();

    if (g_failures == 0) {
        printf("spotlight_test: all assertions held\n");
        return 0;
    }
    printf("spotlight_test: %d FAILURES\n", g_failures);
    return 1;
}
