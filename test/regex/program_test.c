// vexspoke/test/regex/program_test.c — owner test for regex/program.
//
// Proves the Thompson emitter at its own seam by hand-building tiny ASTs and
// inspecting the emitted instruction stream: the SAVE 0 / body / SAVE 1 / MATCH
// frame, concatenation order, the SPLIT+JUMP alternation and STAR/PLUS/QUEST
// back-edges with their patch targets, capture SAVE slots around a group,
// repetition expansion, out-of-range roots, the instruction ceiling, and the
// borrow/free lifecycle.

#include "vex.h"
#include <stdio.h>

#include "regex/program.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Adds a node and returns its index.
static u32 addNode(RegexNodeList *nodes, u8 kind) {
    return RegexNodeList_add(nodes, kind);
}

// Builds a two-child node (concat or alternation).
static u32 addPair(RegexNodeList *nodes, u8 kind, u32 a, u32 b) {
    u32 index = addNode(nodes, kind);
    RegexNode *node = RegexNodeList_get(nodes, index);
    (*node).a = a;
    (*node).b = b;
    return index;
}

// Checks the outer frame and a single literal.
static fn testFrame() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 character = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, character)).byte = (u8) 'a';

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_count(&program) == 0);
    CHECK(RegexProgram_build(&program, &nodes, character));
    CHECK(RegexProgram_count(&program) == 4);

    const RegexInst *save0 = RegexProgram_at(&program, 0);
    const RegexInst *body = RegexProgram_at(&program, 1);
    const RegexInst *save1 = RegexProgram_at(&program, 2);
    const RegexInst *match = RegexProgram_at(&program, 3);
    CHECK(save0 && (*save0).op == REGEX_OP_SAVE && (*save0).a == 0);
    CHECK(body && (*body).op == REGEX_OP_CHAR && (*body).byte == (u8) 'a');
    CHECK(save1 && (*save1).op == REGEX_OP_SAVE && (*save1).a == 1);
    CHECK(match && (*match).op == REGEX_OP_MATCH);
    CHECK(RegexProgram_at(&program, 4) == nullptr);

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks that concatenation emits left operand first.
static fn testConcat() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 b = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, b)).byte = (u8) 'b';
    u32 cat = addPair(&nodes, REGEX_NODE_CONCAT, a, b);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, cat));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *one = RegexProgram_at(&program, 1);
    const RegexInst *two = RegexProgram_at(&program, 2);
    CHECK(one && (*one).op == REGEX_OP_CHAR && (*one).byte == (u8) 'a');
    CHECK(two && (*two).op == REGEX_OP_CHAR && (*two).byte == (u8) 'b');

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks the alternation SPLIT/JUMP wiring and its patch targets.
static fn testAlternation() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 b = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, b)).byte = (u8) 'b';
    u32 alt = addPair(&nodes, REGEX_NODE_ALT, a, b);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, alt));
    CHECK(RegexProgram_count(&program) == 7);

    const RegexInst *split = RegexProgram_at(&program, 1);
    const RegexInst *jump = RegexProgram_at(&program, 3);
    CHECK(split && (*split).op == REGEX_OP_SPLIT);
    CHECK((*split).a == 2);          // the first branch body
    CHECK((*split).b == 4);          // the second branch body
    CHECK(jump && (*jump).op == REGEX_OP_JUMP);
    CHECK((*jump).a == 5);           // past the second branch, to SAVE 1
    CHECK((*RegexProgram_at(&program, 5)).op == REGEX_OP_SAVE);

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks STAR/PLUS/QUEST back-edges and their exits.
static fn testQuantifiers() {
    RegexNodeList nodes;

    // a* -> SAVE0, SPLIT(1->2, else 4), CHAR, JUMP->1, SAVE1, MATCH
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 star = addNode(&nodes, REGEX_NODE_STAR);
    (*RegexNodeList_get(&nodes, star)).a = a;

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, star));
    CHECK(RegexProgram_count(&program) == 6);
    const RegexInst *split = RegexProgram_at(&program, 1);
    const RegexInst *jump = RegexProgram_at(&program, 3);
    CHECK(split && (*split).op == REGEX_OP_SPLIT && (*split).a == 2 && (*split).b == 4);
    CHECK(jump && (*jump).op == REGEX_OP_JUMP && (*jump).a == 1);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a+ -> SAVE0, CHAR, SPLIT(->body, else), SAVE1, MATCH
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 plus = addNode(&nodes, REGEX_NODE_PLUS);
    (*RegexNodeList_get(&nodes, plus)).a = a;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, plus));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *plusSplit = RegexProgram_at(&program, 2);
    CHECK(plusSplit && (*plusSplit).op == REGEX_OP_SPLIT);
    CHECK((*plusSplit).a == 1);   // back to the body
    CHECK((*plusSplit).b == 3);   // exit to SAVE 1
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a? -> SAVE0, SPLIT(body, else), CHAR, SAVE1, MATCH
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 quest = addNode(&nodes, REGEX_NODE_QUEST);
    (*RegexNodeList_get(&nodes, quest)).a = a;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, quest));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *questSplit = RegexProgram_at(&program, 1);
    CHECK(questSplit && (*questSplit).op == REGEX_OP_SPLIT);
    CHECK((*questSplit).a == 2 && (*questSplit).b == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks capture SAVE slots and repetition expansion.
static fn testGroupAndRepeat() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 group = addNode(&nodes, REGEX_NODE_GROUP);
    RegexNode *groupNode = RegexNodeList_get(&nodes, group);
    (*groupNode).a = a;
    (*groupNode).group = 1;

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, group));
    // SAVE0, SAVE2, CHAR, SAVE3, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 6);
    const RegexInst *open = RegexProgram_at(&program, 1);
    const RegexInst *close = RegexProgram_at(&program, 3);
    CHECK(open && (*open).op == REGEX_OP_SAVE && (*open).a == 2);
    CHECK(close && (*close).op == REGEX_OP_SAVE && (*close).a == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a{2,3} -> two mandatory copies then one optional copy (three CHARs).
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    u32 repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    RegexNode *repeatNode = RegexNodeList_get(&nodes, repeat);
    (*repeatNode).a = a;
    (*repeatNode).repMin = 2;
    (*repeatNode).repMax = 3;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, repeat));
    // SAVE0, CHAR, CHAR, SPLIT, CHAR, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 7);
    int chars = 0;
    for (u32 i = 0; i < RegexProgram_count(&program); i++) {
        const RegexInst *inst = RegexProgram_at(&program, i);
        if ((*inst).op == REGEX_OP_CHAR)
            chars++;
    }
    CHECK(chars == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // {2,} -> two mandatory copies then a star of the body.
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, a)).byte = (u8) 'a';
    repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    repeatNode = RegexNodeList_get(&nodes, repeat);
    (*repeatNode).a = a;
    (*repeatNode).repMin = 2;
    (*repeatNode).repMax = REGEX_REP_INF;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, repeat));
    // SAVE0, CHAR, CHAR, SPLIT, CHAR, JUMP, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 8);
    const RegexInst *starJump = RegexProgram_at(&program, 5);
    CHECK(starJump && (*starJump).op == REGEX_OP_JUMP && (*starJump).a == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks refusal paths: a bad root and the instruction ceiling.
static fn testRefusal() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 character = addNode(&nodes, REGEX_NODE_CHAR);

    RegexProgram program;
    program = RegexProgram();
    // An out-of-range root is not emittable.
    CHECK(!RegexProgram_build(&program, &nodes, 999));
    RegexProgram_free(&program);

    // A repetition bound far past the instruction ceiling refuses cleanly. The
    // parser caps {m,n} at 1000; the program ceiling is the second line of
    // defence, so it is driven directly here.
    nodes = RegexNodeList();
    character = addNode(&nodes, REGEX_NODE_CHAR);
    (*RegexNodeList_get(&nodes, character)).byte = (u8) 'a';
    u32 repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    RegexNode *repeatNode = RegexNodeList_get(&nodes, repeat);
    (*repeatNode).a = character;
    (*repeatNode).repMin = 0;
    (*repeatNode).repMax = REGEX_MAX_PROGRAM + 100u;
    program = RegexProgram();
    CHECK(!RegexProgram_build(&program, &nodes, repeat));
    RegexProgram_free(&program);
    CHECK(program.insts == nullptr && program.count == 0);

    // The unit is reusable after a refusal.
    nodes = RegexNodeList();
    character = addNode(&nodes, REGEX_NODE_CHAR);
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, character));
    CHECK(RegexProgram_count(&program) == 4);
    RegexProgram_free(&program);
    RegexProgram_free(&program); // d64 free is a no-op
    CHECK(program.insts == nullptr);

    RegexNodeList_free(&nodes);
}

int main() {
    testFrame();
    testConcat();
    testAlternation();
    testQuantifiers();
    testGroupAndRepeat();
    testRefusal();

    if (g_failures == 0) {
        printf("program_test: all assertions held\n");
        return 0;
    }
    printf("program_test: %d FAILURES\n", g_failures);
    return 1;
}
