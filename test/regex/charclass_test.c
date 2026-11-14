// vexspoke/test/regex/charclass_test.c — owner test for regex/charclass.
//
// Proves the 256-bit byte-membership bitmap: clear, single bytes, inclusive
// ranges, ASCII case folding at insert time, the backslash-d/w/s families and
// their complements, negation, and the boundary bytes 0x00 and 0xFF.

#include <stddef.h>
#include <stdio.h>

#include "regex/charclass.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks an empty class, then single-byte membership.
static fn testBytes() {
    RegexCharClass cls;
    RegexCharClass_clear(&cls);
    CHECK(!RegexCharClass_test(&cls, (u8) 'a'));
    CHECK(!RegexCharClass_test(&cls, 0));
    CHECK(!RegexCharClass_test(&cls, 255));

    RegexCharClass_addByte(&cls, (u8) 'a', false);
    CHECK(RegexCharClass_test(&cls, (u8) 'a'));
    CHECK(!RegexCharClass_test(&cls, (u8) 'b'));
    CHECK(!RegexCharClass_test(&cls, (u8) 'A')); // case folding is opt-in

    // Boundary bytes live at both ends of the bitmap.
    RegexCharClass edge;
    RegexCharClass_clear(&edge);
    RegexCharClass_addByte(&edge, 0, false);
    RegexCharClass_addByte(&edge, 255, false);
    CHECK(RegexCharClass_test(&edge, 0));
    CHECK(RegexCharClass_test(&edge, 255));
    CHECK(!RegexCharClass_test(&edge, 1));
    CHECK(!RegexCharClass_test(&edge, 254));
}

// Checks inclusive ranges and their folding.
static fn testRanges() {
    RegexCharClass cls;
    RegexCharClass_clear(&cls);
    RegexCharClass_addRange(&cls, (u8) 'a', (u8) 'z', false);
    CHECK(RegexCharClass_test(&cls, (u8) 'a'));
    CHECK(RegexCharClass_test(&cls, (u8) 'm'));
    CHECK(RegexCharClass_test(&cls, (u8) 'z'));
    CHECK(!RegexCharClass_test(&cls, (u8) 'A'));
    CHECK(!RegexCharClass_test(&cls, (u8) '{'));

    // An inverted range adds nothing rather than looping.
    RegexCharClass inverted;
    RegexCharClass_clear(&inverted);
    RegexCharClass_addRange(&inverted, (u8) 'z', (u8) 'a', false);
    CHECK(!RegexCharClass_test(&inverted, (u8) 'z'));
    CHECK(!RegexCharClass_test(&inverted, (u8) 'a'));

    // Folding brings the opposite case in for letters only.
    RegexCharClass folded;
    RegexCharClass_clear(&folded);
    RegexCharClass_addByte(&folded, (u8) 'A', true);
    CHECK(RegexCharClass_test(&folded, (u8) 'A'));
    CHECK(RegexCharClass_test(&folded, (u8) 'a'));

    RegexCharClass digit;
    RegexCharClass_clear(&digit);
    RegexCharClass_addByte(&digit, (u8) '5', true); // not a letter: no fold
    CHECK(RegexCharClass_test(&digit, (u8) '5'));
    CHECK(!RegexCharClass_test(&digit, (u8) 'a'));
}

// Checks the \d \D \w \W \s \S families and the letter predicate.
static fn testPredefined() {
    CHECK(RegexCharClass_isPredefined('d'));
    CHECK(RegexCharClass_isPredefined('W'));
    CHECK(RegexCharClass_isPredefined('s'));
    CHECK(!RegexCharClass_isPredefined('q'));
    CHECK(!RegexCharClass_isPredefined('\\'));

    RegexCharClass digits;
    RegexCharClass_clear(&digits);
    RegexCharClass_addPredefined(&digits, 'd');
    CHECK(RegexCharClass_test(&digits, (u8) '0'));
    CHECK(RegexCharClass_test(&digits, (u8) '9'));
    CHECK(!RegexCharClass_test(&digits, (u8) 'a'));

    RegexCharClass notDigits;
    RegexCharClass_clear(&notDigits);
    RegexCharClass_addPredefined(&notDigits, 'D');
    CHECK(!RegexCharClass_test(&notDigits, (u8) '0'));
    CHECK(RegexCharClass_test(&notDigits, (u8) 'a'));

    RegexCharClass word;
    RegexCharClass_clear(&word);
    RegexCharClass_addPredefined(&word, 'w');
    CHECK(RegexCharClass_test(&word, (u8) 'a'));
    CHECK(RegexCharClass_test(&word, (u8) 'Z'));
    CHECK(RegexCharClass_test(&word, (u8) '7'));
    CHECK(RegexCharClass_test(&word, (u8) '_'));
    CHECK(!RegexCharClass_test(&word, (u8) '-'));

    RegexCharClass notWord;
    RegexCharClass_clear(&notWord);
    RegexCharClass_addPredefined(&notWord, 'W');
    CHECK(!RegexCharClass_test(&notWord, (u8) 'a'));
    CHECK(RegexCharClass_test(&notWord, (u8) '-'));

    RegexCharClass space;
    RegexCharClass_clear(&space);
    RegexCharClass_addPredefined(&space, 's');
    CHECK(RegexCharClass_test(&space, (u8) ' '));
    CHECK(RegexCharClass_test(&space, (u8) '\t'));
    CHECK(RegexCharClass_test(&space, (u8) '\n'));
    CHECK(!RegexCharClass_test(&space, (u8) 'x'));

    RegexCharClass notSpace;
    RegexCharClass_clear(&notSpace);
    RegexCharClass_addPredefined(&notSpace, 'S');
    CHECK(!RegexCharClass_test(&notSpace, (u8) ' '));
    CHECK(RegexCharClass_test(&notSpace, (u8) 'x'));

    // An unknown letter is a no-op, not a wildcard.
    RegexCharClass unknown;
    RegexCharClass_clear(&unknown);
    RegexCharClass_addPredefined(&unknown, 'q');
    CHECK(!RegexCharClass_test(&unknown, (u8) 'q'));
}

// Checks negation over the whole 256-bit space.
static fn testNegate() {
    RegexCharClass cls;
    RegexCharClass_clear(&cls);
    RegexCharClass_addByte(&cls, (u8) 'a', false);
    RegexCharClass_negate(&cls);
    CHECK(!RegexCharClass_test(&cls, (u8) 'a'));
    CHECK(RegexCharClass_test(&cls, (u8) 'b'));
    CHECK(RegexCharClass_test(&cls, 0));
    CHECK(RegexCharClass_test(&cls, 255));

    RegexCharClass_negate(&cls);
    CHECK(RegexCharClass_test(&cls, (u8) 'a'));
    CHECK(!RegexCharClass_test(&cls, (u8) 'b'));
}

int main() {
    testBytes();
    testRanges();
    testPredefined();
    testNegate();

    if (g_failures == 0) {
        printf("charclass_test: all assertions held\n");
        return 0;
    }
    printf("charclass_test: %d FAILURES\n", g_failures);
    return 1;
}
