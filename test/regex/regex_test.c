// vexspoke/test/regex/regex_test.c — owner test for regex/regex.
//
// Proves the compiled byte regex end to end:
//   - literals and leftmost substring search, with find offsets;
//   - greedy quantifiers (* + ? {m} {m,} {m,n}) and alternation;
//   - capturing groups, group offsets, unmatched groups (REGEX_UNSET),
//     and non-capturing (?:...);
//   - character classes [abc] [^abc] [a-z] and the \d \w \s families;
//   - escapes (\n \t \. \\ \xHH \0) and the anchors ^ $ (with REGEX_MULTILINE);
//   - the REGEX_CASE_INSENSITIVE / DOTALL / MULTILINE flags;
//   - isMatch / isFullMatch / find / findGroups (incl. truncation flag);
//   - every getter, both string projections (incl. truncation + null safety);
//   - loud compile rejection (null pattern, unbalanced bracket, dangling
//     quantifier, bad range, unsupported group, malformed \x), recovery after
//     a rejection, and the emitted [vex] diagnostic;
//   - bounded progress: a catastrophic-backtracking-shaped pattern must return
//     a clean non-match, not hang (the Pike VM is linear);
//   - nullptr safety on every entry point.

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "regex/regex.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Compiles a pattern, runs a body with it, then frees it. Keeps the cases terse.
#define WITH(pattern, flags)                                               \
    for (Regex *re = Regex_2((pattern), (flags)); re != nullptr; Regex_free(re), re = nullptr)

// Redirects stderr into a scratch file so a THROW diagnostic can be asserted.
static int captureStart(char *path, bytes pathCap) {
    snprintf(path, pathCap, "%s/regex_stderr_%d.txt", "/tmp", (int) getpid());
    int saved = dup(STDERR_FILENO);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (saved < 0 || fd < 0)
        return -1;
    dup2(fd, STDERR_FILENO);
    close(fd);
    return saved;
}

// Restores stderr and reads back what was captured.
static bytes captureStop(int saved, const char *path, char *out, bytes outCap) {
    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(saved);
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    bytes n = fread(out, 1, outCap - 1u, f);
    out[n] = '\0';
    fclose(f);
    unlink(path);
    return n;
}

// Checks literal search, offsets, and empty/longer pattern edges.
static void testLiteralAndFind(void) {
    Regex *re = Regex_1("hello");
    CHECK(re != nullptr);
    CHECK(Regex_isMatch(re, "say hello world"));
    CHECK(!Regex_isMatch(re, "say goodbye"));
    CHECK(Regex_pattern(re) != nullptr && strcmp(Regex_pattern(re), "hello") == 0);
    CHECK(Regex_flags(re) == REGEX_DEFAULT);
    CHECK(Regex_groupCount(re) == 0);
    CHECK(Regex_instCount(re) >= 4); // SAVE CHAR*5 SAVE MATCH
    CHECK(Regex_classCount(re) == 0);

    RegexSpan match;
    CHECK(Regex_find(re, "say hello", 9, &match));
    CHECK(match.start == 4 && match.end == 9);
    CHECK(!Regex_find(re, "nope", 4, &match));
    Regex_free(re);

    Regex *empty = Regex_1("");
    CHECK(Regex_isMatch(empty, "anything"));
    CHECK(Regex_find(empty, "xy", 2, &match) && match.start == 0 && match.end == 0);
    CHECK(Regex_isFullMatch(empty, ""));
    CHECK(!Regex_isFullMatch(empty, "x"));
    Regex_free(empty);

    Regex *dot = Regex(".");
    CHECK(Regex_isMatch(dot, "a"));
    CHECK(!Regex_isMatch(dot, ""));
    Regex_free(dot);
}

// Checks the greedy quantifiers and their bounded behavior.
static void testQuantifiers(void) {
    WITH("colou?r", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "color"));
        CHECK(Regex_isFullMatch(re, "colour"));
        CHECK(!Regex_isFullMatch(re, "colouur"));
    }
    WITH("ab*c", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "ac"));
        CHECK(Regex_isFullMatch(re, "abbbc"));
        CHECK(!Regex_isFullMatch(re, "adc"));
    }
    WITH("ab+c", REGEX_DEFAULT) {
        CHECK(!Regex_isFullMatch(re, "ac"));
        CHECK(Regex_isFullMatch(re, "abc"));
    }
    WITH("a{3}", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "aaa"));
        CHECK(!Regex_isFullMatch(re, "aa"));
        CHECK(!Regex_isFullMatch(re, "aaaa"));
    }
    WITH("a{2,4}", REGEX_DEFAULT) {
        CHECK(!Regex_isFullMatch(re, "a"));
        CHECK(Regex_isFullMatch(re, "aa"));
        CHECK(Regex_isFullMatch(re, "aaaa"));
        CHECK(!Regex_isFullMatch(re, "aaaaa"));
    }
    WITH("a{2,}", REGEX_DEFAULT) {
        CHECK(!Regex_isFullMatch(re, "a"));
        CHECK(Regex_isFullMatch(re, "aaaa"));
    }
    // Greedy: '*|$'-free check that the longest leftmost match is reported.
    WITH("a+", REGEX_DEFAULT) {
        RegexSpan m;
        CHECK(Regex_find(re, "baaab", 5, &m) && m.start == 1 && m.end == 4);
    }
}

// Checks alternation, captures, unmatched groups, and non-capturing groups.
static void testGroupsAndAlternation(void) {
    WITH("cat|dog", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "cat"));
        CHECK(Regex_isFullMatch(re, "dog"));
        CHECK(!Regex_isFullMatch(re, "cow"));
    }
    WITH("(a)(b.sh)", REGEX_DEFAULT) {
        CHECK(Regex_groupCount(re) == 2);
        RegexSpan spans[3];
        bool truncated = true;
        CHECK(Regex_findGroups(re, "ab", 2, spans, 3, &truncated));
        CHECK(!truncated);
        CHECK(spans[0].start == 0 && spans[0].end == 2);
        CHECK(spans[1].start == 0 && spans[1].end == 1);
        CHECK(spans[2].start == 1 && spans[2].end == 2);
    }
    WITH("(a)?b.sh", REGEX_DEFAULT) {
        RegexSpan spans[2];
        CHECK(Regex_findGroups(re, "b.sh", 1, spans, 2, nullptr));
        CHECK(spans[1].start == REGEX_UNSET && spans[1].end == REGEX_UNSET);
        CHECK(Regex_findGroups(re, "ab", 2, spans, 2, nullptr));
        CHECK(spans[1].start == 0 && spans[1].end == 1);
    }
    WITH("(?:ab)+", REGEX_DEFAULT) {
        CHECK(Regex_groupCount(re) == 0);
        CHECK(Regex_isFullMatch(re, "ababab"));
    }
    WITH("(a+)(b.sh+)", REGEX_DEFAULT) {
        RegexSpan spans[3];
        CHECK(Regex_findGroups(re, "aabb", 4, spans, 3, nullptr));
        CHECK(spans[1].start == 0 && spans[1].end == 2);
        CHECK(spans[2].start == 2 && spans[2].end == 4);
    }
    // Truncated group buffer reports the cut rather than silently dropping it.
    WITH("(a)(b.sh)(c)", REGEX_DEFAULT) {
        RegexSpan spans[2];
        bool truncated = false;
        CHECK(Regex_findGroups(re, "abc", 3, spans, 2, &truncated));
        CHECK(truncated);
        CHECK(spans[0].start == 0 && spans[0].end == 3);
    }
}

// Checks character classes, ranges, negation, and the \d \w \s families.
static void testClasses(void) {
    WITH("[abc]+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "cab"));
        CHECK(!Regex_isFullMatch(re, "cad"));
        CHECK(Regex_classCount(re) == 1);
    }
    WITH("[^abc]", REGEX_DEFAULT) {
        CHECK(!Regex_isMatch(re, "abc"));
        CHECK(Regex_isMatch(re, "abz"));
    }
    WITH("[a-z]+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "hello"));
        CHECK(!Regex_isFullMatch(re, "Hello"));
    }
    WITH("[0-9]{3}", REGEX_DEFAULT) {
        CHECK(Regex_isMatch(re, "abc123"));
        CHECK(!Regex_isMatch(re, "abc12"));
    }
    WITH("\\d+", REGEX_DEFAULT) {
        CHECK(Regex_isMatch(re, "abc123"));
        CHECK(!Regex_isMatch(re, "abc"));
    }
    WITH("\\w+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "a_b-c") == false);
        CHECK(Regex_isFullMatch(re, "a_b9"));
    }
    WITH("\\s", REGEX_DEFAULT) {
        CHECK(Regex_isMatch(re, "a b.sh"));
        CHECK(Regex_isMatch(re, "a\tb.sh"));
        CHECK(!Regex_isMatch(re, "ab"));
    }
    WITH("\\D+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "abc"));
        CHECK(!Regex_isFullMatch(re, "ab1"));
    }
    // ']' as the first class member is a literal.
    WITH("[]x]+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "x]"));
    }
    // '-' at the edges is a literal.
    WITH("[a-]+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "a-a"));
    }
}

// Checks escapes and anchors.
static void testEscapesAndAnchors(void) {
    WITH("\\d+\\.\\d+", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "3.14"));
        CHECK(!Regex_isFullMatch(re, "314"));
    }
    WITH("a\\nb", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "a\nb.sh"));
    }
    WITH("\\x41", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "A"));
    }
    WITH("a\\tb", REGEX_DEFAULT) {
        CHECK(Regex_isFullMatch(re, "a\tb.sh"));
    }
    WITH("^abc", REGEX_DEFAULT) {
        CHECK(Regex_isMatch(re, "abcdef"));
        CHECK(Regex_isFullMatch(re, "abc"));
        CHECK(!Regex_isFullMatch(re, "xabc"));
    }
    WITH("b.sh$", REGEX_DEFAULT) {
        CHECK(Regex_isMatch(re, "ab"));
        CHECK(!Regex_isMatch(re, "abx"));
    }
    // MENU under REGEX_MULTILINE.
    WITH("^b.sh", REGEX_MULTILINE) {
        CHECK(Regex_isMatch(re, "a\nb.sh"));
    }
    WITH("^b.sh", REGEX_DEFAULT) {
        CHECK(!Regex_isMatch(re, "a\nb.sh"));
    }
    // '$' via multiline matches an interior line end.
    WITH("a$", REGEX_MULTILINE) {
        CHECK(Regex_isMatch(re, "a\nb.sh"));
    }
}

// Checks the three flags independently.
static void testFlags(void) {
    WITH("hello", REGEX_CASE_INSENSITIVE) {
        CHECK(Regex_isMatch(re, "HELLO"));
        CHECK(Regex_flags(re) == REGEX_CASE_INSENSITIVE);
    }
    WITH("hello", REGEX_DEFAULT) {
        CHECK(!Regex_isMatch(re, "HELLO"));
    }
    WITH("[a-z]+", REGEX_CASE_INSENSITIVE) {
        CHECK(Regex_isFullMatch(re, "HeLLo"));
    }
    WITH("a.b.sh", REGEX_DEFAULT) {
        CHECK(!Regex_isMatch(re, "a\nb.sh"));
    }
    WITH("a.b.sh", REGEX_DOTALL) {
        CHECK(Regex_isMatch(re, "a\nb.sh"));
    }
}

// Checks both string projections, their truncation flag, and null safety.
static void testProjections(void) {
    Regex *re = Regex_2("(ab)+", REGEX_CASE_INSENSITIVE);
    CHECK(re != nullptr);

    char text[128];
    bool truncated = true;
    CHECK(Regex_toString(re, text, sizeof(text), &truncated));
    CHECK(!truncated);
    CHECK(strstr(text, "/(ab)+/i") == text);
    CHECK(strstr(text, "groups=1") != nullptr);

    char tiny[8];
    truncated = false;
    CHECK(!Regex_toString(re, tiny, sizeof(tiny), &truncated));
    CHECK(truncated);

    char dump[512];
    truncated = true;
    CHECK(Regex_toStringStruct(re, dump, sizeof(dump), &truncated));
    CHECK(!truncated);
    CHECK(strstr(dump, "pattern=\"(ab)+\"") != nullptr);
    CHECK(strstr(dump, "groupCount=1") != nullptr);
    CHECK(strstr(dump, "program=set") != nullptr);
    Regex_free(re);

    CHECK(Regex_toString(nullptr, text, sizeof(text), nullptr));
    CHECK(strcmp(text, "nullptr") == 0);
    CHECK(Regex_toStringStruct(nullptr, text, sizeof(text), nullptr));
    CHECK(strcmp(text, "nullptr") == 0);
    CHECK(!Regex_toString(nullptr, text, 0, nullptr));
    CHECK(!Regex_toString(re, nullptr, 8, nullptr));
}

// Checks the loud compile rejections and that the [vex] diagnostic is emitted.
static void testRejection(void) {
    char path[256];
    char captured[512];
    int saved = captureStart(path, sizeof(path));

    CHECK(Regex_2(nullptr, 0) == nullptr);
    CHECK(Regex_1("(") == nullptr);        // missing ')'
    CHECK(Regex_1("a)") == nullptr);       // stray ')'
    CHECK(Regex_1("[a") == nullptr);       // missing ']'
    CHECK(Regex_1("[z-a]") == nullptr);    // inverted range
    CHECK(Regex_1("*a") == nullptr);       // nothing to repeat
    CHECK(Regex_1("(?=a)") == nullptr);    // unsupported group construct
    CHECK(Regex_1("a{3,1}") == nullptr);   // inverted repetition
    CHECK(Regex_1("a{2") == nullptr);      // malformed repetition
    CHECK(Regex_1("\\x1") == nullptr);     // malformed hex escape

    if (saved >= 0) {
        bytes n = captureStop(saved, path, captured, sizeof(captured));
        CHECK(n > 0);
        CHECK(strstr(captured, "[vex]") != nullptr);
        CHECK(strstr(captured, "regex compile") != nullptr);
    } else {
        CHECK(0 && "stderr capture unavailable");
    }

    // Recovery: a valid pattern still compiles after a run of rejections.
    Regex *re = Regex_1("[a-z0-9]+");
    CHECK(re != nullptr);
    CHECK(Regex_isFullMatch(re, "abc123"));
    Regex_free(re);
}

// Checks bounded progress: the classic blow-up shape must not hang.
static void testBoundedProgress(void) {
    const char *hostile = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    Regex *re = Regex_1("(a+)+b.sh");
    CHECK(re != nullptr);
    CHECK(!Regex_isMatch(re, hostile)); // no 'b.sh' -- a clean, fast non-match

    // A long subject with a real match stays linear too.
    char longSubject[4097];
    for (bytes i = 0; i < 4000; i++)
        longSubject[i] = 'a';
    longSubject[4000] = 'b';
    longSubject[4001] = '\0';
    Regex *run = Regex_1("a+b.sh");
    CHECK(run != nullptr);
    RegexSpan m;
    CHECK(Regex_find(run, longSubject, 4001, &m));
    CHECK(m.start == 0 && m.end == 4001);

    // Determinism: the same input yields the same result every run.
    RegexSpan again;
    CHECK(Regex_find(run, longSubject, 4001, &again));
    CHECK(again.start == m.start && again.end == m.end);

    Regex_free(run);
    Regex_free(re);
}

// Verifies null inputs are rejected by every entry point with safe results.
static void testNullSafety(void) {
    Regex *re = Regex_1("x");
    CHECK(!Regex_isMatch(nullptr, "x"));
    CHECK(!Regex_isMatch(re, nullptr));
    CHECK(!Regex_isFullMatch(nullptr, "x"));
    CHECK(!Regex_isFullMatch(re, nullptr));
    CHECK(!Regex_find(re, nullptr, 0, nullptr));
    CHECK(!Regex_findGroups(re, "x", 1, nullptr, 0, nullptr));
    RegexSpan span;
    CHECK(!Regex_find(nullptr, "x", 1, &span));
    CHECK(Regex_groupCount(nullptr) == 0);
    CHECK(Regex_instCount(nullptr) == 0);
    CHECK(Regex_classCount(nullptr) == 0);
    CHECK(Regex_flags(nullptr) == 0);
    CHECK(strcmp(Regex_pattern(nullptr), "") == 0);
    Regex_free(nullptr); // must not crash
    Regex_free(re);
}

// Checks the documented safety ceilings reject loudly instead of half-compiling.
static void testSafetyBounds(void) {
    char path[256];
    char captured[16384];
    int saved = captureStart(path, sizeof(path));

    // Repetition bound.
    CHECK(Regex_1("a{1001}") == nullptr);
    CHECK(Regex_1("a{1,1001}") == nullptr);
    static char thousand[1001];
    for (bytes i = 0; i < 1000; i++)
        thousand[i] = 'a';
    thousand[1000] = '\0';
    Regex *maxRepeat = Regex_1("a{1000}");
    CHECK(maxRepeat != nullptr);
    CHECK(Regex_isFullMatch(maxRepeat, thousand));
    Regex_free(maxRepeat);

    // Group nesting bound.
    char deep[512];
    bytes d = 0;
    for (int i = 0; i < 200; i++)
        deep[d++] = '(';
    deep[d++] = 'a';
    for (int i = 0; i < 200; i++)
        deep[d++] = ')';
    deep[d] = '\0';
    CHECK(Regex_1(deep) == nullptr);

    // AST node ceiling: a long flat literal blows past the node budget.
    static char flat[9001];
    for (bytes i = 0; i < 9000; i++)
        flat[i] = 'a';
    flat[9000] = '\0';
    CHECK(Regex_1(flat) == nullptr);

    // ...but a comfortably sized literal still compiles and matches.
    static char midsize[2001];
    for (bytes i = 0; i < 2000; i++)
        midsize[i] = 'a';
    midsize[2000] = '\0';
    Regex *fine = Regex_1(midsize);
    CHECK(fine != nullptr);
    CHECK(Regex_isFullMatch(fine, midsize));
    Regex_free(fine);

    // Program ceiling: repetition expansion past the instruction budget.
    char blown[512];
    bytes b = 0;
    for (int i = 0; i < 40; i++)
        b += (bytes) snprintf(blown + b, sizeof(blown) - b, "a{1000}");
    CHECK(Regex_1(blown) == nullptr);

    // A near-ceiling class run still compiles (the bound is not hair-trigger).
    static char classes[12001];
    bytes c = 0;
    for (int i = 0; i < 4000; i++) {
        classes[c++] = '[';
        classes[c++] = 'a';
        classes[c++] = ']';
    }
    classes[c] = '\0';
    Regex *many = Regex_1(classes);
    CHECK(many != nullptr);
    CHECK(Regex_classCount(many) == 4000);
    Regex_free(many);

    if (saved >= 0) {
        bytes n = captureStop(saved, path, captured, sizeof(captured));
        CHECK(n > 0);
        CHECK(strstr(captured, "repetition count too large") != nullptr);
        CHECK(strstr(captured, "pattern nesting too deep") != nullptr);
        CHECK(strstr(captured, "node ceiling") != nullptr);
        CHECK(strstr(captured, "program too large") != nullptr);
    }
}

// Runs the regex owner cases.
static void testFullAlternatives(void) {
    const char *patterns[] = {"a|ab", "|ab", "(a|ab)", "a?|ab", "a*|ab"};
    for (bytes i = 0; i < sizeof patterns / sizeof patterns[0]; i++) {
        Regex *re = Regex(patterns[i]);
        CHECK(re != nullptr);
        CHECK(Regex_isFullMatch(re, "ab"));
        CHECK(!Regex_isFullMatch(re, "abc"));
        Regex_free(re);
    }
    // Search still prefers the first alternative; full admission is distinct.
    Regex *re = Regex("a|ab");
    RegexSpan span;
    CHECK(Regex_find(re, "ab", 2, &span));
    CHECK(span.start == 0 && span.end == 1);
    CHECK(Regex_isFullMatch(re, "a"));
    CHECK(!Regex_isFullMatch(re, ""));
    Regex_free(re);
}

static void testStrictSyntax(void) {
    const char *patterns[] = {"a+?", "a++", "a**", "a{2}{3}", "a{2,3}?",
                              "\\b.sh", "\\B", "\\1", "\\q", "[\\b.sh]"};
    for (bytes i = 0; i < sizeof patterns / sizeof patterns[0]; i++) {
        char path[256], captured[512];
        int saved = captureStart(path, sizeof(path));
        CHECK(saved >= 0);
        Regex *re = Regex(patterns[i]);
        CHECK(re == nullptr);
        Regex_free(re);
        if (saved >= 0) {
            captureStop(saved, path, captured, sizeof(captured));
            CHECK(strstr(captured, "[vex]") != nullptr);
            CHECK(strstr(captured, "unsupported") != nullptr);
            char *first = strchr(captured, '\n');
            CHECK(first && first[1] == '\0'); // exactly one diagnostic
        }
    }
    Regex *re = Regex("\\+\\?\\\\");
    CHECK(re != nullptr);
    CHECK(Regex_isFullMatch(re, "+?\\"));
    Regex_free(re);
    re = Regex("\\n\\x41");
    CHECK(Regex_isFullMatch(re, "\nA"));
    Regex_free(re);
}

int main(void) {
    testFullAlternatives();
    testStrictSyntax();
    testLiteralAndFind();
    testQuantifiers();
    testGroupsAndAlternation();
    testClasses();
    testEscapesAndAnchors();
    testFlags();
    testProjections();
    testRejection();
    testBoundedProgress();
    testSafetyBounds();
    testNullSafety();

    if (g_failures == 0) {
        printf("regex_test: all assertions held\n");
        return 0;
    }
    printf("regex_test: %d FAILURES\n", g_failures);
    return 1;
}
