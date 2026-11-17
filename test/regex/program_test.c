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

static i32 g_failures = 0;

#define CHECK(cond)                                                        \

;;GETTER
static inline u32 RegexNode_a(const RegexNode *s) { return (*s).a; }
;;GETTER
static inline u32 *RegexNode_aMut(RegexNode *s) { return &(*s).a; }
;;SETTER
static inline fn RegexNode_setA(RegexNode *s, u32 v) { (*s).a = v; }
;;GETTER
static inline u32 RegexNode_b(const RegexNode *s) { return (*s).b; }
;;GETTER
static inline u32 *RegexNode_bMut(RegexNode *s) { return &(*s).b; }
;;SETTER
static inline fn RegexNode_setB(RegexNode *s, u32 v) { (*s).b = v; }
;;GETTER
static inline u8 RegexNode_byte(const RegexNode *s) { return (*s).byte; }
;;GETTER
static inline u8 *RegexNode_byteMut(RegexNode *s) { return &(*s).byte; }
;;SETTER
static inline fn RegexNode_setByte(RegexNode *s, u8 v) { (*s).byte = v; }
;;GETTER
static inline u32 RegexNode_group(const RegexNode *s) { return (*s).group; }
;;GETTER
static inline u32 *RegexNode_groupMut(RegexNode *s) { return &(*s).group; }
;;SETTER
static inline fn RegexNode_setGroup(RegexNode *s, u32 v) { (*s).group = v; }
;;GETTER
static inline u32 RegexNode_repMax(const RegexNode *s) { return (*s).repMax; }
;;GETTER
static inline u32 *RegexNode_repMaxMut(RegexNode *s) { return &(*s).repMax; }
;;SETTER
static inline fn RegexNode_setRepMax(RegexNode *s, u32 v) { (*s).repMax = v; }
;;GETTER
static inline u32 RegexNode_repMin(const RegexNode *s) { return (*s).repMin; }
;;GETTER
static inline u32 *RegexNode_repMinMut(RegexNode *s) { return &(*s).repMin; }
;;SETTER
static inline fn RegexNode_setRepMin(RegexNode *s, u32 v) { (*s).repMin = v; }

;;GETTER
static inline u32 RegexInst_a(const RegexInst *s) { return (*s).a; }
;;GETTER
static inline u32 *RegexInst_aMut(RegexInst *s) { return &(*s).a; }
;;SETTER
static inline fn RegexInst_setA(RegexInst *s, u32 v) { (*s).a = v; }
;;GETTER
static inline u32 RegexInst_b(const RegexInst *s) { return (*s).b; }
;;GETTER
static inline u32 *RegexInst_bMut(RegexInst *s) { return &(*s).b; }
;;SETTER
static inline fn RegexInst_setB(RegexInst *s, u32 v) { (*s).b = v; }
;;GETTER
static inline u8 RegexInst_byte(const RegexInst *s) { return (*s).byte; }
;;GETTER
static inline u8 *RegexInst_byteMut(RegexInst *s) { return &(*s).byte; }
;;SETTER
static inline fn RegexInst_setByte(RegexInst *s, u8 v) { (*s).byte = v; }
;;GETTER
static inline u8 RegexInst_op(const RegexInst *s) { return (*s).op; }
;;GETTER
static inline u8 *RegexInst_opMut(RegexInst *s) { return &(*s).op; }
;;SETTER
static inline fn RegexInst_setOp(RegexInst *s, u8 v) { (*s).op = v; }

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
    RegexNode_setA(node, a);
    RegexNode_setB(node, b);
    return index;
}

// Checks the outer frame and a single literal.
static fn testFrame() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 character = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, character), (u8) 'a');

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_count(&program) == 0);
    CHECK(RegexProgram_build(&program, &nodes, character));
    CHECK(RegexProgram_count(&program) == 4);

    const RegexInst *save0 = RegexProgram_at(&program, 0);
    const RegexInst *body = RegexProgram_at(&program, 1);
    const RegexInst *save1 = RegexProgram_at(&program, 2);
    const RegexInst *match = RegexProgram_at(&program, 3);
    CHECK(save0 && RegexInst_op(save0) == REGEX_OP_SAVE && RegexInst_a(save0) == 0);
    CHECK(body && RegexInst_op(body) == REGEX_OP_CHAR && RegexInst_byte(body) == (u8) 'a');
    CHECK(save1 && RegexInst_op(save1) == REGEX_OP_SAVE && RegexInst_a(save1) == 1);
    CHECK(match && RegexInst_op(match) == REGEX_OP_MATCH);
    CHECK(RegexProgram_at(&program, 4) == nullptr);

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks that concatenation emits left operand first.
static fn testConcat() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 b = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, b), (u8) 'b');
    u32 cat = addPair(&nodes, REGEX_NODE_CONCAT, a, b);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, cat));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *one = RegexProgram_at(&program, 1);
    const RegexInst *two = RegexProgram_at(&program, 2);
    CHECK(one && RegexInst_op(one) == REGEX_OP_CHAR && RegexInst_byte(one) == (u8) 'a');
    CHECK(two && RegexInst_op(two) == REGEX_OP_CHAR && RegexInst_byte(two) == (u8) 'b');

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks the alternation SPLIT/JUMP wiring and its patch targets.
static fn testAlternation() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 b = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, b), (u8) 'b');
    u32 alt = addPair(&nodes, REGEX_NODE_ALT, a, b);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, alt));
    CHECK(RegexProgram_count(&program) == 7);

    const RegexInst *split = RegexProgram_at(&program, 1);
    const RegexInst *jump = RegexProgram_at(&program, 3);
    CHECK(split && RegexInst_op(split) == REGEX_OP_SPLIT);
    CHECK(RegexInst_a(split) == 2);          // the first branch body
    CHECK(RegexInst_b(split) == 4);          // the second branch body
    CHECK(jump && RegexInst_op(jump) == REGEX_OP_JUMP);
    CHECK(RegexInst_a(jump) == 5);           // past the second branch, to SAVE 1
    CHECK(RegexInst_op(RegexProgram_at(&program, 5)) == REGEX_OP_SAVE);

    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks STAR/PLUS/QUEST back-edges and their exits.
static fn testQuantifiers() {
    RegexNodeList nodes;

    // a* -> SAVE0, SPLIT(1->2, else 4), CHAR, JUMP->1, SAVE1, MATCH
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 star = addNode(&nodes, REGEX_NODE_STAR);
    RegexNode_setA(RegexNodeList_get(&nodes, star), a);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, star));
    CHECK(RegexProgram_count(&program) == 6);
    const RegexInst *split = RegexProgram_at(&program, 1);
    const RegexInst *jump = RegexProgram_at(&program, 3);
    CHECK(split && RegexInst_op(split) == REGEX_OP_SPLIT && RegexInst_a(split) == 2 && RegexInst_b(split) == 4);
    CHECK(jump && RegexInst_op(jump) == REGEX_OP_JUMP && RegexInst_a(jump) == 1);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a+ -> SAVE0, CHAR, SPLIT(->body, else), SAVE1, MATCH
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 plus = addNode(&nodes, REGEX_NODE_PLUS);
    RegexNode_setA(RegexNodeList_get(&nodes, plus), a);
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, plus));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *plusSplit = RegexProgram_at(&program, 2);
    CHECK(plusSplit && RegexInst_op(plusSplit) == REGEX_OP_SPLIT);
    CHECK(RegexInst_a(plusSplit) == 1);   // back to the body
    CHECK(RegexInst_b(plusSplit) == 3);   // exit to SAVE 1
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a? -> SAVE0, SPLIT(body, else), CHAR, SAVE1, MATCH
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 quest = addNode(&nodes, REGEX_NODE_QUEST);
    RegexNode_setA(RegexNodeList_get(&nodes, quest), a);
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, quest));
    CHECK(RegexProgram_count(&program) == 5);
    const RegexInst *questSplit = RegexProgram_at(&program, 1);
    CHECK(questSplit && RegexInst_op(questSplit) == REGEX_OP_SPLIT);
    CHECK(RegexInst_a(questSplit) == 2 && RegexInst_b(questSplit) == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);
}

// Checks capture SAVE slots and repetition expansion.
static fn testGroupAndRepeat() {
    RegexNodeList nodes;
    nodes = RegexNodeList();
    u32 a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 group = addNode(&nodes, REGEX_NODE_GROUP);
    RegexNode *groupNode = RegexNodeList_get(&nodes, group);
    RegexNode_setA(groupNode, a);
    RegexNode_setGroup(groupNode, 1);

    RegexProgram program;
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, group));
    // SAVE0, SAVE2, CHAR, SAVE3, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 6);
    const RegexInst *open = RegexProgram_at(&program, 1);
    const RegexInst *close = RegexProgram_at(&program, 3);
    CHECK(open && RegexInst_op(open) == REGEX_OP_SAVE && RegexInst_a(open) == 2);
    CHECK(close && RegexInst_op(close) == REGEX_OP_SAVE && RegexInst_a(close) == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // a{2,3} -> two mandatory copies then one optional copy (three CHARs).
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    u32 repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    RegexNode *repeatNode = RegexNodeList_get(&nodes, repeat);
    RegexNode_setA(repeatNode, a);
    RegexNode_setRepMin(repeatNode, 2);
    RegexNode_setRepMax(repeatNode, 3);
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, repeat));
    // SAVE0, CHAR, CHAR, SPLIT, CHAR, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 7);
    i32 chars = 0;
    for (u32 i = 0; i < RegexProgram_count(&program); i++) {
        const RegexInst *inst = RegexProgram_at(&program, i);
        if (RegexInst_op(inst) == REGEX_OP_CHAR)
            chars++;
    }
    CHECK(chars == 3);
    RegexProgram_free(&program);
    RegexNodeList_free(&nodes);

    // {2,} -> two mandatory copies then a star of the body.
    nodes = RegexNodeList();
    a = addNode(&nodes, REGEX_NODE_CHAR);
    RegexNode_setByte(RegexNodeList_get(&nodes, a), (u8) 'a');
    repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    repeatNode = RegexNodeList_get(&nodes, repeat);
    RegexNode_setA(repeatNode, a);
    RegexNode_setRepMin(repeatNode, 2);
    RegexNode_setRepMax(repeatNode, REGEX_REP_INF);
    program = RegexProgram();
    CHECK(RegexProgram_build(&program, &nodes, repeat));
    // SAVE0, CHAR, CHAR, SPLIT, CHAR, JUMP, SAVE1, MATCH
    CHECK(RegexProgram_count(&program) == 8);
    const RegexInst *starJump = RegexProgram_at(&program, 5);
    CHECK(starJump && RegexInst_op(starJump) == REGEX_OP_JUMP && RegexInst_a(starJump) == 3);
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
    RegexNode_setByte(RegexNodeList_get(&nodes, character), (u8) 'a');
    u32 repeat = addNode(&nodes, REGEX_NODE_REPEAT);
    RegexNode *repeatNode = RegexNodeList_get(&nodes, repeat);
    RegexNode_setA(repeatNode, character);
    RegexNode_setRepMin(repeatNode, 0);
    RegexNode_setRepMax(repeatNode, REGEX_MAX_PROGRAM + 100u);
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

i32 main() {
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
