// vexspoke/test/search/find_test.c — owner test for search/find.
//
// Proves the IDE-style finder:
//   - substring search: case-insensitive by default, case-sensitive with the
//     flag; offsets from findFirst; empty pattern -> 0; longer pattern -> -1;
//   - whole-word matching: bounded by non-alnum (underscore counts as a word
//     char), start/end boundaries, and punctuation delimiters;
//   - SQL LIKE: '%' (0+ chars, including middle and both ends), '_' (exactly
//     one char), case folding, and empty pattern rejection;
//   - nullptr safety on every entry point.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "search/find.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks case modes, find offsets, empty patterns, and substring boundary misses.
static fn test_substring() {
    CHECK(Search_match("Hello World", "world", FIND_DEFAULT));            // case-insensitive
    CHECK(!Search_match("Hello World", "world", FIND_CASE_SENSITIVE));
    CHECK(Search_match("Hello World", "World", FIND_CASE_SENSITIVE));
    CHECK(Search_match("Hello World", "Hello", FIND_CASE_SENSITIVE));

    CHECK(Search_findFirst("Hello World", "World", FIND_CASE_SENSITIVE) == 6);
    CHECK(Search_findFirst("Hello World", "world", FIND_DEFAULT) == 6);
    CHECK(Search_findFirst("abcabc", "bc", FIND_DEFAULT) == 1);
    CHECK(Search_findFirst("abcabc", "z", FIND_DEFAULT) == -1);
    CHECK(Search_findFirst("abc", "abcd", FIND_DEFAULT) == -1);           // longer
    CHECK(Search_findFirst("abc", "", FIND_DEFAULT) == 0);                // empty pattern
    CHECK(Search_findFirst("", "", FIND_DEFAULT) == 0);
    CHECK(Search_findFirst("", "a", FIND_DEFAULT) == -1);
}

// Checks whole-word delimiters and flag dispatch with both case-sensitivity modes.
static fn test_exact_word() {
    CHECK(Search_exactWord("the cat sat", "cat", true));
    CHECK(Search_exactWord("cat", "cat", true));
    CHECK(Search_exactWord("cat.", "cat", true));                         // punctuation boundary
    CHECK(Search_exactWord("(cat)", "cat", true));
    CHECK(!Search_exactWord("concatenate", "cat", true));                 // embedded
    CHECK(!Search_exactWord("catapult", "cat", true));                    // no right boundary
    CHECK(!Search_exactWord("bobcat", "cat", true));                      // no left boundary
    CHECK(!Search_exactWord("a_cat", "cat", true));                       // '_' is a word char

    CHECK(Search_match("the cat sat", "CAT", FIND_EXACT_WORD));           // default case-insensitive
    CHECK(!Search_match("the cat sat", "CAT", FIND_EXACT_WORD | FIND_CASE_SENSITIVE));
    CHECK(Search_match("the Cat sat", "Cat", FIND_EXACT_WORD | FIND_CASE_SENSITIVE));
    CHECK(Search_findFirst("a cat b.sh", "cat", FIND_EXACT_WORD) == 2);
    CHECK(Search_findFirst("scatter", "cat", FIND_EXACT_WORD) == -1);
}

// Checks SQL LIKE percent/underscore matching, case policy, and dispatch flags.
static fn test_like() {
    CHECK(Search_like("hello", "hello", false));
    CHECK(Search_like("hello", "h%o", false));
    CHECK(Search_like("hello", "h%", false));
    CHECK(Search_like("hello", "%o", false));
    CHECK(Search_like("hello", "%ell%", false));
    CHECK(Search_like("hello", "%", false));                              // matches anything
    CHECK(Search_like("hello", "h_llo", false));                          // '_' = one char
    CHECK(!Search_like("hello", "h_lo", false));                          // '_' = exactly one
    CHECK(!Search_like("hello", "h_lloo", false));
    CHECK(Search_like("hello", "H%O", false));                            // case-insensitive
    CHECK(!Search_like("hello", "H%O", true));
    CHECK(Search_like("hello", "H%O", true) == false);
    CHECK(!Search_like("hello", "", false));                              // empty pattern

    CHECK(Search_match("hello", "h%o", FIND_LIKE_WILDCARD));
    CHECK(Search_match("HELLO", "h%o", FIND_LIKE_WILDCARD));
    CHECK(!Search_match("HELLO", "h%o", FIND_LIKE_WILDCARD | FIND_CASE_SENSITIVE));
    CHECK(Search_match("hello", "h_llo", FIND_LIKE_WILDCARD));
}

// Verifies null inputs are rejected by each search operation with safe results.
static fn test_nulls() {
    CHECK(!Search_match(nullptr, "x", FIND_DEFAULT));
    CHECK(!Search_match("x", nullptr, FIND_DEFAULT));
    CHECK(!Search_match(nullptr, nullptr, FIND_DEFAULT));
    CHECK(Search_findFirst(nullptr, "x", FIND_DEFAULT) == -1);
    CHECK(Search_findFirst("x", nullptr, FIND_DEFAULT) == -1);
    CHECK(!Search_like(nullptr, "x", false));
    CHECK(!Search_like("x", nullptr, false));
    CHECK(!Search_exactWord(nullptr, "x", false));
    CHECK(!Search_exactWord("x", nullptr, false));
}

// Runs substring, exact-word, wildcard, and null-input finder cases.
int main() {
    test_substring();
    test_exact_word();
    test_like();
    test_nulls();

    if (g_failures == 0) {
        printf("find_test: all assertions held\n");
        return 0;
    }
    printf("find_test: %d FAILURES\n", g_failures);
    return 1;
}
