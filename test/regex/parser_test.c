// vexspoke/test/regex/parser_test.c — owner test for regex/parser.
//
// Proves the recursive-descent parser through its own seam: it builds an AST
// with the expected root shape and group count, folds case when asked, fills
// class bitmaps, records one reason per rejection (unbalanced bracket, dangling
// quantifier, inverted range/repetition, unsupported group, malformed escape,
// stray ')'), enforces the documented ceilings, and transfers its class array
// exactly once.

#include "vex.h"
#include <stdio.h>
#include <string.h>

#include "regex/parser.h"

static i32 g_failures = 0;

;;GETTER
static inline u32 RegexNode_a(const RegexNode *s) { return (*s).a; }
;;GETTER
static inline u32 RegexNode_b(const RegexNode *s) { return (*s).b; }
;;GETTER
static inline u8 RegexNode_byte(const RegexNode *s) { return (*s).byte; }
;;GETTER
static inline u32 RegexNode_clsIndex(const RegexNode *s) { return (*s).clsIndex; }
;;GETTER
static inline u8 RegexNode_kind(const RegexNode *s) { return (*s).kind; }
;;GETTER
static inline u32 RegexNode_repMax(const RegexNode *s) { return (*s).repMax; }
;;GETTER
static inline u32 RegexNode_repMin(const RegexNode *s) { return (*s).repMin; }

;;GETTER
static inline u32 RegexNodeList_count(const RegexNodeList *s) { return (*s).count; }

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Parses a pattern with a fresh parser and reports the root and status.
static u32 parse(const char *pattern, bool ignoreCase, RegexParser *parser) {
    *parser = RegexParser(pattern, strlen(pattern), ignoreCase);
    return RegexParser_parse(parser);
}

// Checks a clean parse: root shape, node access, group count and error state.
static fn testShapes() {
    RegexParser parser;

    // A single literal folds (or not) and reports a CHAR root.
    CHECK(parse("a", false, &parser) != REGEX_NODE_NONE);
    CHECK(RegexParser_ok(&parser));
    CHECK(RegexParser_error(&parser) == nullptr);
    CHECK(RegexParser_groupCount(&parser) == 0);
    RegexNodeList *nodes = RegexParser_nodes(&parser);
    CHECK(nodes != nullptr);
    CHECK(RegexNodeList_count(nodes) >= 1);
    const RegexNode *root = RegexNodeList_at(nodes, 0);
    CHECK(root != nullptr);
    CHECK(RegexNode_kind(root) == REGEX_NODE_CHAR);
    CHECK(RegexNode_byte(root) == (u8) 'a');
    RegexParser_free(&parser);

    // ignoreCase folds the literal at parse time.
    CHECK(parse("A", true, &parser) != REGEX_NODE_NONE);
    const RegexNode *folded = RegexNodeList_at(RegexParser_nodes(&parser), 0);
    CHECK(folded != nullptr);
    CHECK(RegexNode_byte(folded) == (u8) 'a');
    RegexParser_free(&parser);

    // Concatenation nests left and keeps the operands as indices.
    CHECK(parse("ab", false, &parser) != REGEX_NODE_NONE);
    RegexNodeList *cat = RegexParser_nodes(&parser);
    CHECK(RegexNodeList_count(cat) == 3);
    const RegexNode *top = RegexNodeList_at(cat, RegexNodeList_count(cat) - 1u);
    CHECK(top != nullptr);
    CHECK(RegexNode_kind(top) == REGEX_NODE_CONCAT);
    const RegexNode *left = RegexNodeList_at(cat, RegexNode_a(top));
    const RegexNode *right = RegexNodeList_at(cat, RegexNode_b(top));
    CHECK(left != nullptr && right != nullptr);
    CHECK(RegexNode_byte(left) == (u8) 'a');
    CHECK(RegexNode_byte(right) == (u8) 'b');
    RegexParser_free(&parser);

    // A capturing group counts; a non-capturing group does not.
    CHECK(parse("(a)", false, &parser) != REGEX_NODE_NONE);
    CHECK(RegexParser_groupCount(&parser) == 1);
    RegexParser_free(&parser);
    CHECK(parse("(?:a)", false, &parser) != REGEX_NODE_NONE);
    CHECK(RegexParser_groupCount(&parser) == 0);
    CHECK((*RegexParser_nodes(&parser)).count == 1);
    RegexParser_free(&parser);

    // The empty pattern is a clean parse (an EMPTY root).
    CHECK(parse("", false, &parser) != REGEX_NODE_NONE);
    CHECK(RegexParser_ok(&parser));
    RegexParser_free(&parser);

    // A repetition carries its bounds.
    CHECK(parse("a{2,4}", false, &parser) != REGEX_NODE_NONE);
    RegexNodeList *rep = RegexParser_nodes(&parser);
    CHECK(RegexNodeList_count(rep) == 2);
    const RegexNode *bounds = RegexNodeList_at(rep, 1);
    CHECK(bounds != nullptr);
    CHECK(RegexNode_kind(bounds) == REGEX_NODE_REPEAT);
    CHECK(RegexNode_repMin(bounds) == 2 && RegexNode_repMax(bounds) == 4);
    RegexParser_free(&parser);

    // {m,} has no upper bound.
    CHECK(parse("a{2,}", false, &parser) != REGEX_NODE_NONE);
    const RegexNode *open = RegexNodeList_at(RegexParser_nodes(&parser), 1);
    CHECK(open != nullptr);
    CHECK(RegexNode_repMin(open) == 2 && RegexNode_repMax(open) == REGEX_REP_INF);
    RegexParser_free(&parser);
}

// Checks class construction and the one-shot class transfer.
static fn testClasses() {
    RegexParser parser;
    CHECK(parse("[a-z]", false, &parser) != REGEX_NODE_NONE);
    CHECK(RegexParser_groupCount(&parser) == 0);
    const RegexNode *node = RegexNodeList_at(RegexParser_nodes(&parser), 0);
    CHECK(node != nullptr);
    CHECK(RegexNode_kind(node) == REGEX_NODE_CLASS);

    u32 classCount = 0;
    RegexCharClass *classes = RegexParser_releaseClasses(&parser, &classCount);
    CHECK(classes != nullptr);
    CHECK(classCount == 1);
    CHECK(RegexCharClass_test(&classes[RegexNode_clsIndex(node)], (u8) 'm'));
    CHECK(!RegexCharClass_test(&classes[RegexNode_clsIndex(node)], (u8) 'M'));

    // The transfer is one-shot: a second call yields nothing.
    u32 again = 7;
    CHECK(RegexParser_releaseClasses(&parser, &again) == nullptr);
    CHECK(again == 0);

    RegexParser_free(&parser);
}

// Checks that every documented rejection records a reason and no root.
static fn testRejections() {
    RegexParser parser;
    struct { const char *pattern; const char *fragment; } cases[] = {
        { "a+?", "quantifier unsupported" },
        { "a++", "quantifier unsupported" },
        { "a**", "quantifier unsupported" },
        { "a{2}{3}", "quantifier unsupported" },
        { "a{2,3}?", "quantifier unsupported" },
        { "\\b", "unsupported escape" },
        { "\\B", "unsupported escape" },
        { "\\1", "unsupported escape" },
        { "\\q", "unsupported escape" },
        { "[\\b]", "unsupported escape" },
        { "(", "missing closing ')'" },
        { "a)", "unexpected ')'" },
        { "[a", "missing closing ']'" },
        { "[z-a]", "invalid character range" },
        { "*a", "nothing to repeat" },
        { "(?=a)", "unsupported group construct" },
        { "a{3,1}", "invalid repetition range" },
        { "a{2", "malformed repetition" },
        { "\\x1", "malformed \\x escape" },
    };
    for (bytes i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        CHECK(parse(cases[i].pattern, false, &parser) == REGEX_NODE_NONE);
        CHECK(!RegexParser_ok(&parser));
        const char *error = RegexParser_error(&parser);
        CHECK(error != nullptr);
        CHECK(error && strstr(error, cases[i].fragment) != nullptr);
        RegexParser_free(&parser);
    }
    // A trailing backslash is its own reason.
    CHECK(parse("\\", false, &parser) == REGEX_NODE_NONE);
    CHECK(!RegexParser_ok(&parser));
    RegexParser_free(&parser);
}

// Checks the documented ceilings refuse rather than half-build.
static fn testCeilings() {
    RegexParser parser;
    CHECK(parse("a{1001}", false, &parser) == REGEX_NODE_NONE);
    CHECK(RegexParser_error(&parser) && strstr(RegexParser_error(&parser), "repetition") != nullptr);
    RegexParser_free(&parser);

    char deep[512];
    bytes d = 0;
    for (i32 i = 0; i < 200; i++) deep[d++] = '(';
    deep[d++] = 'a';
    for (i32 i = 0; i < 200; i++) deep[d++] = ')';
    deep[d] = '\0';
    parser = RegexParser(deep, d, false);
    CHECK(RegexParser_parse(&parser) == REGEX_NODE_NONE);
    CHECK(RegexParser_error(&parser) && strstr(RegexParser_error(&parser), "nesting") != nullptr);
    RegexParser_free(&parser);

    static char flat[9001];
    for (bytes i = 0; i < 9000; i++) flat[i] = 'a';
    flat[9000] = '\0';
    parser = RegexParser(flat, 9000u, false);
    CHECK(RegexParser_parse(&parser) == REGEX_NODE_NONE);
    CHECK(RegexParser_error(&parser) && strstr(RegexParser_error(&parser), "node ceiling") != nullptr);
    RegexParser_free(&parser);

    // A near-ceiling class run still parses.
    static char classes[12001];
    bytes c = 0;
    for (i32 i = 0; i < 4000; i++) {
        classes[c++] = '[';
        classes[c++] = 'a';
        classes[c++] = ']';
    }
    classes[c] = '\0';
    parser = RegexParser(classes, c, false);
    CHECK(RegexParser_parse(&parser) != REGEX_NODE_NONE);
    u32 count = 0;
    RegexCharClass *released = RegexParser_releaseClasses(&parser, &count);
    CHECK(released != nullptr);
    CHECK(count == 4000);
    RegexParser_free(&parser);
}

// Checks init/free are reusable and null-safe across cycles.
static fn testLifecycle() {
    RegexParser parser;
    for (i32 i = 0; i < 3; i++) {
        CHECK(parse("abc", false, &parser) != REGEX_NODE_NONE);
        RegexParser_free(&parser);
        CHECK((*RegexParser_nodes(&parser)).count == 0);
    }
    RegexParser_free(&parser);
    RegexParser_free(&parser);
}

i32 main() {
    testShapes();
    testClasses();
    testRejections();
    testCeilings();
    testLifecycle();

    if (g_failures == 0) {
        printf("parser_test: all assertions held\n");
        return 0;
    }
    printf("parser_test: %d FAILURES\n", g_failures);
    return 1;
}
