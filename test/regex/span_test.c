// vexspoke/test/regex/span_test.c — owner test for regex/span.
//
// Proves the RegexSpan value: the "did not participate" span, the participation
// predicate, and the measured length across set, unset and inverted spans.

#include <stddef.h>
#include <stdio.h>

#include "regex/span.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks the unset span, its predicate, and its zero length.
static void testUnset(void) {
    RegexSpan none = RegexSpan_none();
    CHECK(none.start == REGEX_UNSET);
    CHECK(none.end == REGEX_UNSET);
    CHECK(!RegexSpan_isSet(none));
    CHECK(RegexSpan_length(none) == 0);
}

// Checks a participating span and its measured length.
static void testSet(void) {
    RegexSpan span;
    span.start = 4;
    span.end = 9;
    CHECK(RegexSpan_isSet(span));
    CHECK(RegexSpan_length(span) == 5);

    // An empty but participating group (start == end) is still a set span.
    RegexSpan empty;
    empty.start = 3;
    empty.end = 3;
    CHECK(RegexSpan_isSet(empty));
    CHECK(RegexSpan_length(empty) == 0);

    // Only one end set is not a span.
    RegexSpan half;
    half.start = 1;
    half.end = REGEX_UNSET;
    CHECK(!RegexSpan_isSet(half));
    CHECK(RegexSpan_length(half) == 0);

    // An inverted span measures zero rather than wrapping.
    RegexSpan inverted;
    inverted.start = 9;
    inverted.end = 4;
    CHECK(RegexSpan_length(inverted) == 0);
}

int main(void) {
    testUnset();
    testSet();

    if (g_failures == 0) {
        printf("span_test: all assertions held\n");
        return 0;
    }
    printf("span_test: %d FAILURES\n", g_failures);
    return 1;
}
