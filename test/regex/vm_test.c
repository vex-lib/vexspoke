// vexspoke/test/regex/vm_test.c — owner test for regex/vm.
//
// Proves the Pike-VM simulator at its own seam: it allocates all scratch up
// front, matches a hand-built program over a subject, reports capture spans
// through RegexVm_span, honours anchored and the three options independently,
// refuses to match past a full consumption, stays linear on the classic
// backtracking blow-up shape, and frees cleanly.

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nio/mem.h"
#include "regex/parser.h"
#include "regex/program.h"
#include "regex/vm.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Compiles a pattern with the real parser and program builder so the VM is
// driven by an honest instruction stream rather than a hand-written one.
static bool compile(const char *pattern, bool ignoreCase, RegexProgram *program,
                    RegexCharClass **classes, u32 *classCount, u32 *groupCount) {
    RegexParser parser = RegexParser(pattern, strlen(pattern), ignoreCase);
    u32 root = RegexParser_parse(&parser);
    bool ok = root != REGEX_NODE_NONE && RegexParser_ok(&parser);
    if (ok) {
        *groupCount = RegexParser_groupCount(&parser);
        ok = RegexProgram_build(program, RegexParser_nodes(&parser), root);
    }
    if (ok)
        *classes = RegexParser_releaseClasses(&parser, classCount);
    RegexParser_free(&parser);
    return ok;
}

// Runs one search and reports the whole-match span. The pattern is folded with
// the same option the VM uses, so the two halves of case-insensitivity are
// always consistent.
static bool search(const char *pattern, const char *subject, bool anchored,
                   RegexOptions options, RegexSpan *outWhole) {
    RegexProgram program = RegexProgram();
    RegexCharClass *classes = nullptr;
    u32 classCount = 0;
    u32 groupCount = 0;
    if (!compile(pattern, options.ignoreCase, &program, &classes, &classCount, &groupCount)) {
        RegexProgram_free(&program);
        return false;
    }
    RegexVm vm = RegexVm(&program, groupCount);
    bool ok = vm.best != nullptr;
    CHECK(ok);
    bool hit = false;
    if (ok) {
        hit = RegexVm_run(&vm, classes, subject, strlen(subject), anchored, options);
        if (hit && outWhole)
            *outWhole = RegexVm_span(&vm, 0);
        RegexVm_free(&vm);
    }
    RegexProgram_free(&program);
    if (classes)
        Memory_free(classes);
    return hit;
}

// A neutral options value (case sensitive, dot excludes newline, single line).
static RegexOptions plain() {
    RegexOptions options = RegexOptions(false, false, false);
    return options;
}

// Checks unanchored search, leftmost offsets, and a full consumption.
static fn testSearch() {
    RegexSpan whole = RegexSpan_none();
    CHECK(search("hello", "say hello", false, plain(), &whole));
    CHECK(whole.start == 4 && whole.end == 9);

    CHECK(!search("hello", "say goodbye", false, plain(), nullptr));
    CHECK(search("a+", "baaab", false, plain(), &whole));
    CHECK(whole.start == 1 && whole.end == 4);

    // Anchored: only a match at offset 0 counts.
    CHECK(search("abc", "abcdef", true, plain(), &whole));
    CHECK(whole.start == 0 && whole.end == 3);
    CHECK(!search("bcd", "abcdef", true, plain(), nullptr));
    CHECK(search("bcd", "abcdef", false, plain(), &whole));
    CHECK(whole.start == 1 && whole.end == 4);

    // The empty pattern matches the empty string.
    CHECK(search("", "", false, plain(), &whole));
    CHECK(whole.start == 0 && whole.end == 0);
}

// Checks the three options independently.
static fn testOptions() {
    RegexOptions options = plain();
    CHECK(!search("HELLO", "hello", false, options, nullptr));
    options.ignoreCase = true;
    CHECK(search("HELLO", "hello", false, options, nullptr));
    CHECK(search("[a-z]+", "HeLLo", false, options, nullptr));

    options = plain();
    CHECK(!search("a.b.sh", "a\nb.sh", false, options, nullptr));
    options.dotall = true;
    CHECK(search("a.b.sh", "a\nb.sh", false, options, nullptr));

    options = plain();
    CHECK(!search("^b.sh", "a\nb.sh", false, options, nullptr));
    options.multiline = true;
    CHECK(search("^b.sh", "a\nb.sh", false, options, nullptr));
    CHECK(search("a$", "a\nb.sh", false, options, nullptr));
}

// Checks capture spans: the whole match plus each group.
static fn testCaptures() {
    RegexProgram program = RegexProgram();
    RegexCharClass *classes = nullptr;
    u32 classCount = 0;
    u32 groupCount = 0;
    CHECK(compile("(a)(b.sh)", false, &program, &classes, &classCount, &groupCount));
    CHECK(groupCount == 2);

    RegexVm vm = RegexVm(&program, groupCount);
    CHECK(vm.best != nullptr);
    CHECK(RegexVm_run(&vm, classes, "ab", 2, false, plain()));
    RegexSpan whole = RegexVm_span(&vm, 0);
    RegexSpan first = RegexVm_span(&vm, 1);
    RegexSpan second = RegexVm_span(&vm, 2);
    CHECK(whole.start == 0 && whole.end == 2);
    CHECK(first.start == 0 && first.end == 1);
    CHECK(second.start == 1 && second.end == 2);

    // An out-of-range group yields the "did not participate" span.
    CHECK(!RegexSpan_isSet(RegexVm_span(&vm, 9)));
    RegexVm_free(&vm);
    RegexProgram_free(&program);
    if (classes)
        Memory_free(classes);

    // An unmatched optional group reports no participation.
    program = RegexProgram();
    classes = nullptr;
    CHECK(compile("(a)?b.sh", false, &program, &classes, &classCount, &groupCount));
    vm = RegexVm(&program, groupCount);
    CHECK(vm.best != nullptr);
    CHECK(RegexVm_run(&vm, classes, "b.sh", 1, false, plain()));
    CHECK(!RegexSpan_isSet(RegexVm_span(&vm, 1)));
    RegexVm_free(&vm);
    RegexProgram_free(&program);
    if (classes)
        Memory_free(classes);
}

// Checks bounded progress on the classic catastrophic-backtracking shape.
static fn testBoundedProgress() {
    RegexSpan whole = RegexSpan_none();
    const char *hostile = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    // (a+)+b.sh cannot match a run of bare a's; a backtracking engine explodes.
    CHECK(!search("(a+)+b.sh", hostile, false, plain(), &whole));

    // A long subject with a real match stays linear.
    char longSubject[4097];
    for (bytes i = 0; i < 4000; i++)
        longSubject[i] = 'a';
    longSubject[4000] = 'b';
    longSubject[4001] = '\0';
    CHECK(search("a+b.sh", longSubject, true, plain(), &whole));
    CHECK(whole.start == 0 && whole.end == 4001);

    // Determinism: the same input yields the same span.
    RegexSpan again = RegexSpan_none();
    CHECK(search("a+b.sh", longSubject, true, plain(), &again));
    CHECK(again.start == whole.start && again.end == whole.end);
}

// Checks the init/free lifecycle across repeated runs and a null-safe free.
static fn testLifecycle() {
    RegexProgram program = RegexProgram();
    RegexCharClass *classes = nullptr;
    u32 classCount = 0;
    u32 groupCount = 0;
    CHECK(compile("ab", false, &program, &classes, &classCount, &groupCount));

    RegexVm vm = RegexVm(&program, groupCount);
    CHECK(vm.best != nullptr);
    for (int i = 0; i < 4; i++) {
        CHECK(RegexVm_run(&vm, classes, "zab", 3, false, plain()));
        CHECK(RegexVm_span(&vm, 0).start == 1);
    }
    RegexVm_free(&vm);
    CHECK(vm.caps == nullptr && vm.program == nullptr);
    RegexVm_free(&vm); // d64 free is a no-op

    RegexProgram_free(&program);
    if (classes)
        Memory_free(classes);
}

// Full admission must keep alternatives alive after a premature MATCH.
static fn testFullAdmission() {
    const char *patterns[] = {"a|ab", "|ab", "(a|ab)", "a?|ab", "a*|ab"};
    for (bytes i = 0; i < sizeof patterns / sizeof patterns[0]; i++) {
        RegexProgram program = RegexProgram();
        RegexCharClass *classes = nullptr;
        u32 classCount = 0, groupCount = 0;
        CHECK(compile(patterns[i], false, &program, &classes, &classCount, &groupCount));
        RegexVm vm = RegexVm(&program, groupCount);
        CHECK(vm.best != nullptr);
        CHECK(RegexVm_runFull(&vm, classes, "ab", 2, plain()));
        RegexSpan span = RegexVm_span(&vm, 0);
        CHECK(span.start == 0 && span.end == 2);
        if (groupCount) {
            span = RegexVm_span(&vm, 1);
            CHECK(span.start == 0 && span.end == 2);
        }
        CHECK(!RegexVm_runFull(&vm, classes, "abc", 3, plain()));
        RegexVm_free(&vm);
        RegexProgram_free(&program);
        Memory_free(classes);
    }
}

int main() {
    testFullAdmission();
    testSearch();
    testOptions();
    testCaptures();
    testBoundedProgress();
    testLifecycle();

    if (g_failures == 0) {
        printf("vm_test: all assertions held\n");
        return 0;
    }
    printf("vm_test: %d FAILURES\n", g_failures);
    return 1;
}
