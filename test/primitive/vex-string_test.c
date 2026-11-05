// vexspoke/test/primitive/vex-string_test.c — owner test for primitive/vex-string.
//
// Proves the arena-backed string primitive:
//   - allocate / allocateBytes / allocateUninitialized (+ the String()
//     chooser) produce NUL-terminated blocks with length, capacity, type,
//     array-form and classId introspection;
//   - copy is independent, equals compares by value, free is null-safe;
//   - compare / contains / indexOf / instring (block and literal forms);
//   - substring / subFirst* / subLast* with clamping and out-of-range starts;
//   - append / appendLiteral / appendLiterals (null-combo identity) and
//     appendInto with an adequate destination;
//   - RECORDED defect: String_appendFirst can never append, because a block's
//     capacity is always length+1 so the `cap >= len+bLen+1` gate is never
//     met for a non-empty suffix. Asserted as a known gap.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "primitive/vex-string.h"
#include "nio/mem.h"
#include "oop/type.h"

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

// Covers string allocation forms, value identity, copying, chooser arities, and freeing.
static void test_allocate(void) {
    CHECK(string_allocate(nullptr) == nullptr);

    uint8_t *s = string_allocate("hello");
    CHECK(s != nullptr);
    CHECK(strcmp(string_get(s), "hello") == 0);
    CHECK(string_length(s) == 5);
    CHECK(string_capacity(s) == 6);                   // length + NUL
    CHECK(string_type(s) == TYPE_STRING_ARRAY);
    CHECK(string_isArray(s));
    CHECK(string_classId() == ID_STRING);
    CHECK(string_equals(s, "hello"));
    CHECK(!string_equals(s, "hell"));
    CHECK(!string_equals(s, "hello!"));
    CHECK(!string_equals(s, nullptr));

    uint8_t *e = string_allocate("");
    CHECK(e != nullptr && string_length(e) == 0 && strcmp(string_get(e), "") == 0);

    uint8_t *b = string_allocateBytes((const uint8_t*) "abc", 3);
    CHECK(b != nullptr && string_length(b) == 3 && strcmp(string_get(b), "abc") == 0);
    uint8_t *z = string_allocateBytes(nullptr, 0);
    CHECK(z != nullptr && string_length(z) == 0);
    CHECK(string_allocateBytes(nullptr, 3) == nullptr);

    uint8_t *u = string_allocateUninitialized(4);
    CHECK(u != nullptr && string_length(u) == 4);
    for (int i = 0; i < 4; i++)
        CHECK(u[i] == 0);
    CHECK(u[4] == '\0');

    // Copy is an independent equal block.
    uint8_t *c = string_copy(s);
    CHECK(c != nullptr && c != s && string_equals(c, "hello"));
    CHECK(string_copy(nullptr) == nullptr);

    // String() chooser forms.
    uint8_t *chooser0 = String();
    CHECK(chooser0 != nullptr && string_length(chooser0) == 0);
    uint8_t *chooser1 = String("world");
    CHECK(chooser1 != nullptr && string_equals(chooser1, "world"));

    string_free(s);
    string_free(e);
    string_free(b);
    string_free(z);
    string_free(u);
    string_free(c);
    string_free(chooser0);
    string_free(chooser1);
    string_free(nullptr);
}

// Checks safe defaults from every string introspection and equality call on null inputs.
static void test_null_introspection(void) {
    CHECK(string_get(nullptr) == nullptr);
    CHECK(string_length(nullptr) == 0);
    CHECK(string_type(nullptr) == 0);
    CHECK(!string_isArray(nullptr));
    CHECK(string_capacity(nullptr) == 0);
    CHECK(!string_equals(nullptr, "x"));
    CHECK(!string_equals(nullptr, nullptr));
}

// Checks ordering, containment, index lookup, in-string matching, and null arguments.
static void test_compare_search(void) {
    uint8_t *a = string_allocate("apple");
    uint8_t *b = string_allocate("banana");
    uint8_t *apple2 = string_allocate("apple");

    CHECK(String_compare(a, b) < 0);
    CHECK(String_compare(b, a) > 0);
    CHECK(String_compare(a, apple2) == 0);
    CHECK(String_compare(nullptr, nullptr) == 0);
    CHECK(String_compare(nullptr, a) < 0);
    CHECK(String_compare(a, nullptr) > 0);

    uint8_t *needle = string_allocate("nan");
    CHECK(String_contains(b, needle));
    CHECK(String_containsLiteral(b, "ban"));
    CHECK(String_containsLiteral(b, "xyz") == false);
    CHECK(!String_contains(nullptr, a));
    CHECK(!String_containsLiteral(b, nullptr));
    string_free(needle);

    CHECK(String_indexOfLiteral(b, "ana") == 1);
    CHECK(String_indexOfLiteral(b, "zzz") == (size_t) -1);
    CHECK(String_indexOf(nullptr, b) == (size_t) -1);

    CHECK(String_instringLiteral(1, b, "ana"));
    CHECK(!String_instringLiteral(0, b, "ana"));
    CHECK(!String_instringLiteral(0, b, "zzz"));

    string_free(a);
    string_free(b);
    string_free(apple2);
}

// Checks substring clamping, empty out-of-range results, and first/last projections.
static void test_substrings(void) {
    uint8_t *s = string_allocate("hello world");

    uint8_t *sub = String_substring(s, 6, 5);
    CHECK(string_equals(sub, "world"));
    // Over-long len clamps to the end.
    uint8_t *clamped = String_substring(s, 6, 999);
    CHECK(string_equals(clamped, "world"));
    // Start at/after length -> empty.
    uint8_t *empty = String_substring(s, 11, 3);
    CHECK(string_length(empty) == 0);
    CHECK(String_substring(nullptr, 0, 1) == nullptr);

    uint8_t *lit = String_substringLiteral("abcdef", 2, 3);
    CHECK(string_equals(lit, "cde"));

    uint8_t *firstChar = String_subFirstChar(s);
    CHECK(string_equals(firstChar, "h"));
    uint8_t *lastChar = String_subLastChar(s);
    CHECK(string_equals(lastChar, "d"));               // 'd' ends "world"
    uint8_t *first = String_subFirst(s, 5);
    CHECK(string_equals(first, "hello"));
    uint8_t *last = String_subLast(s, 5);
    CHECK(string_equals(last, "world"));
    // count >= length returns a full copy.
    uint8_t *all = String_subLast(s, 999);
    CHECK(string_equals(all, "hello world"));

    string_free(s);
    string_free(sub);
    string_free(clamped);
    string_free(empty);
    string_free(lit);
    string_free(firstChar);
    string_free(lastChar);
    string_free(first);
    string_free(last);
    string_free(all);
}

// Checks append variants, null identities, destination capacity, and append-first growth.
static void test_append(void) {
    uint8_t *a = string_allocate("foo");
    uint8_t *b = string_allocate("bar");

    uint8_t *ab = String_append(a, b);
    CHECK(string_equals(ab, "foobar"));
    // Null combos.
    CHECK(string_equals(String_append(a, nullptr), "foo"));
    CHECK(string_equals(String_append(nullptr, b), "bar"));
    CHECK(string_length(String_append(nullptr, nullptr)) == 0);

    uint8_t *lit = String_appendLiteral(a, "baz");
    CHECK(string_equals(lit, "bazfoo") == false);      // appends, not prepends
    CHECK(string_equals(lit, "foobaz"));
    CHECK(string_equals(String_appendLiterals("ab", "cd"), "abcd"));
    CHECK(string_equals(String_appendLiterals(nullptr, "cd"), "cd"));
    CHECK(string_equals(String_appendLiterals("ab", nullptr), "ab"));

    // appendInto with an adequate destination (capacity == length+1 == 7).
    uint8_t *dest = string_allocateUninitialized(6);
    String_appendInto(a, b, dest);
    CHECK(string_equals(dest, "foobar"));
    // Too-small destination is left untouched.
    uint8_t *small = string_allocateUninitialized(3);
    memcpy(small, "xy", 2);
    small[2] = '\0';
    String_appendInto(a, b, small);
    CHECK(memcmp(small, "xy", 2) == 0);

    // appendFirst grows (and may move) the block; the result is reassigned.
    uint8_t *inplace = string_allocate("ab");          // capacity 3
    inplace = String_appendFirst(inplace, b);          // b.sh == "bar" -> grow
    CHECK(string_equals(inplace, "abbar"));
    inplace = String_appendFirstLiteral(inplace, "c");
    CHECK(string_equals(inplace, "abbarc"));
    CHECK(string_equals(String_appendFirst(inplace, nullptr), "abbarc"));
    CHECK(string_equals(String_appendFirst(nullptr, b), "bar"));

    string_free(a);
    string_free(b);
    string_free(ab);
    string_free(lit);
    string_free(dest);
    string_free(small);
    string_free(inplace);
}

// Initializes the memory substrate, runs each string contract case, and returns aggregate failures.
int main(void) {
    CHECK(Memory_init(0));
    test_allocate();
    test_null_introspection();
    test_compare_search();
    test_substrings();
    test_append();

    if (g_failures == 0) {
        printf("string_test: all assertions held\n");
        return 0;
    }
    printf("string_test: %d FAILURES\n", g_failures);
    return 1;
}
