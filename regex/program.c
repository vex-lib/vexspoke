// regex/program.c — the RegexProgram Thompson emitter.

#include "vex.h"
#include "regex/program.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RegexProgram
 * ============================================================================
 * The compiled form of a pattern: one flat array of instructions that a
 * matcher can walk, produced here from the AST by Thompson's construction.
 * Thompson is the right construction for this engine because it never needs
 * to choose between alternatives at run time — alternation becomes a SPLIT,
 * repetition becomes a back-jump, and a capture becomes a SAVE — so the
 * resulting program has no backtracking behaviour to lose control of. That is
 * what lets the matcher stay linear on a hostile pattern (the Bounded Wait
 * Law), and it is why the AST is lowered here rather than interpreted.
 *
 * Emission is deliberately recursive over the AST: the parser caps group
 * nesting at REGEX_MAX_DEPTH, so the C stack depth is bounded by the pattern
 * that was already admitted. Repetition is expanded by copying the body —
 * {m,n} is m mandatory copies plus (n-m) optional ones — and the expansion is
 * capped by REGEX_MAX_PROGRAM, so a huge bound is a refusal, not an OOM.
 *
 * memory: one arena block stamped TYPE_REGEX, doubling on demand. The program
 * borrows the AST for the duration of build and owns nothing else.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RegexProgram (regex/program.c — defined in regex/program.h)
 * ============================================================================
 * The flat instruction array a pattern compiles to, plus the Thompson
 * construction that emits it.
 *
 * STRUCT FIELDS (Mirroring regex/program.h):
 * ----------------------------------------------------------------------------
 *   RegexProgram {
 *     RegexInst *insts;    // engine-arena block of `capacity` instructions
 *     u32   count;    // instructions in use
 *     u32   capacity; // instructions allocated
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - RegexProgram() creates an empty program value
 *   - RegexProgram_build(program, nodes, root)
 *   - RegexProgram_count(program)
 *   - RegexProgram_at(program, index)
 *   - RegexProgram_free(program)
 *
 * Private Core Functions: (.c static)
 *   - programGrow(block, bytes) / emit(program, op, byte, a, b.sh)
 *   - instAt(program, index) / emitNode(program, nodes, index)
 *
 * Admission, ownership, failure:
 *   - init zeroes the program; build emits SAVE 0, the body, SAVE 1, MATCH and
 *     returns false on ceiling breach or arena refusal (a partially emitted
 *     program must be freed, never matched).
 *   - at is a borrowed view valid until the next build or free.
 *   - free releases the block and zeroes the program; null-safe.
 * ============================================================================
 */


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
;;GETTER
static inline u16 RegexInst_reserved(const RegexInst *s) { return (*s).reserved; }
;;GETTER
static inline u16 *RegexInst_reservedMut(RegexInst *s) { return &(*s).reserved; }
;;SETTER
static inline fn RegexInst_setReserved(RegexInst *s, u16 v) { (*s).reserved = v; }

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
static inline u32 RegexNode_clsIndex(const RegexNode *s) { return (*s).clsIndex; }
;;GETTER
static inline u32 *RegexNode_clsIndexMut(RegexNode *s) { return &(*s).clsIndex; }
;;SETTER
static inline fn RegexNode_setClsIndex(RegexNode *s, u32 v) { (*s).clsIndex = v; }
;;GETTER
static inline u32 RegexNode_group(const RegexNode *s) { return (*s).group; }
;;GETTER
static inline u32 *RegexNode_groupMut(RegexNode *s) { return &(*s).group; }
;;SETTER
static inline fn RegexNode_setGroup(RegexNode *s, u32 v) { (*s).group = v; }
;;GETTER
static inline u8 RegexNode_kind(const RegexNode *s) { return (*s).kind; }
;;GETTER
static inline u8 *RegexNode_kindMut(RegexNode *s) { return &(*s).kind; }
;;SETTER
static inline fn RegexNode_setKind(RegexNode *s, u8 v) { (*s).kind = v; }
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
static inline u32 RegexProgram_capacity(const RegexProgram *s) { return (*s).capacity; }
;;GETTER
static inline u32 *RegexProgram_capacityMut(RegexProgram *s) { return &(*s).capacity; }
;;SETTER
static inline fn RegexProgram_setCapacity(RegexProgram *s, u32 v) { (*s).capacity = v; }
;;GETTER
static inline u32 RegexProgram_count(const RegexProgram *s) { return (*s).count; }
;;GETTER
static inline u32 *RegexProgram_countMut(RegexProgram *s) { return &(*s).count; }
;;SETTER
static inline fn RegexProgram_setCount(RegexProgram *s, u32 v) { (*s).count = v; }
;;GETTER
static inline RegexInst * RegexProgram_insts(const RegexProgram *s) { return (*s).insts; }
;;GETTER
static inline RegexInst * *RegexProgram_instsMut(RegexProgram *s) { return &(*s).insts; }
;;SETTER
static inline fn RegexProgram_setInsts(RegexProgram *s, RegexInst * v) { (*s).insts = v; }

#define REGEX_PROGRAM_SEED_CAPACITY 64u

static any programGrow(any block, bytes bytes) {
    if (block)
        return Memory_realloc(block, bytes);
    return Memory_alloc(TYPE_REGEX, bytes);
}

// Appends one instruction; returns its index, or UINT32_MAX on refusal.
static u32 emit(RegexProgram *program, u8 op, u8 byte, u32 a, u32 b) {
    if (RegexProgram_count(program) >= REGEX_MAX_PROGRAM)
        return UINT32_MAX;
    if (RegexProgram_count(program) == RegexProgram_capacity(program)) {
        u32 next = RegexProgram_capacity(program) ? RegexProgram_capacity(program) * 2u : REGEX_PROGRAM_SEED_CAPACITY;
        if (next > REGEX_MAX_PROGRAM)
            next = REGEX_MAX_PROGRAM;
        RegexInst *grown = (RegexInst*) programGrow(RegexProgram_insts(program),
                                                   (bytes) next * sizeof(RegexInst));
        if (!grown)
            return UINT32_MAX;
        RegexProgram_setInsts(program, grown);
        RegexProgram_setCapacity(program, next);
    }
    RegexInst *inst = &RegexProgram_instsMut(program)[RegexProgram_count(program)];
    RegexInst_setOp(inst, op);
    RegexInst_setByte(inst, byte);
    RegexInst_setReserved(inst, 0);
    RegexInst_setA(inst, a);
    RegexInst_setB(inst, b);
    return (*RegexProgram_countMut(program))++;
}

static RegexInst *instAt(RegexProgram *program, u32 index) {
    return &RegexProgram_instsMut(program)[index];
}

// Emits the Thompson program for one AST node. Recursion is bounded by AST
// nesting (capped by REGEX_MAX_DEPTH); concatenation and alternation spines
// recurse once per level but their node count is capped by REGEX_MAX_NODES.
static bool emitNode(RegexProgram *program, const RegexNodeList *nodes, u32 index) {
    const RegexNode *node = RegexNodeList_at(nodes, index);
    if (!node)
        return false;
    switch (RegexNode_kind(node)) {
    case REGEX_NODE_EMPTY:
        return true;
    case REGEX_NODE_CHAR:
        return emit(program, REGEX_OP_CHAR, RegexNode_byte(node), 0, 0) != UINT32_MAX;
    case REGEX_NODE_ANY:
        return emit(program, REGEX_OP_ANY, 0, 0, 0) != UINT32_MAX;
    case REGEX_NODE_CLASS:
        return emit(program, REGEX_OP_CLASS, 0, RegexNode_clsIndex(node), 0) != UINT32_MAX;
    case REGEX_NODE_BOL:
        return emit(program, REGEX_OP_BOL, 0, 0, 0) != UINT32_MAX;
    case REGEX_NODE_EOL:
        return emit(program, REGEX_OP_EOL, 0, 0, 0) != UINT32_MAX;
    case REGEX_NODE_CONCAT:
        return emitNode(program, nodes, RegexNode_a(node)) && emitNode(program, nodes, RegexNode_b(node));
    case REGEX_NODE_ALT: {
        u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
        if (split == UINT32_MAX)
            return false;
        (*instAt(program, split)).a = split + 1u;
        if (!emitNode(program, nodes, RegexNode_a(node)))
            return false;
        u32 jump = emit(program, REGEX_OP_JUMP, 0, 0, 0);
        if (jump == UINT32_MAX)
            return false;
        (*instAt(program, split)).b = RegexProgram_count(program);
        if (!emitNode(program, nodes, RegexNode_b(node)))
            return false;
        (*instAt(program, jump)).a = RegexProgram_count(program);
        return true;
    }
    case REGEX_NODE_STAR: {
        u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
        if (split == UINT32_MAX)
            return false;
        (*instAt(program, split)).a = split + 1u;
        if (!emitNode(program, nodes, RegexNode_a(node)))
            return false;
        u32 jump = emit(program, REGEX_OP_JUMP, 0, 0, 0);
        if (jump == UINT32_MAX)
            return false;
        (*instAt(program, jump)).a = split;
        (*instAt(program, split)).b = RegexProgram_count(program);
        return true;
    }
    case REGEX_NODE_PLUS: {
        u32 start = RegexProgram_count(program);
        if (!emitNode(program, nodes, RegexNode_a(node)))
            return false;
        u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
        if (split == UINT32_MAX)
            return false;
        (*instAt(program, split)).a = start;
        (*instAt(program, split)).b = RegexProgram_count(program);
        return true;
    }
    case REGEX_NODE_QUEST: {
        u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
        if (split == UINT32_MAX)
            return false;
        (*instAt(program, split)).a = split + 1u;
        if (!emitNode(program, nodes, RegexNode_a(node)))
            return false;
        (*instAt(program, split)).b = RegexProgram_count(program);
        return true;
    }
    case REGEX_NODE_REPEAT: {
        u32 min = RegexNode_repMin(node);
        u32 max = RegexNode_repMax(node);
        for (u32 i = 0; i < min; i++)
            if (!emitNode(program, nodes, RegexNode_a(node)))
                return false;
        if (max == REGEX_REP_INF) {
            // {m,} is m copies followed by a star of the body.
            u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
            if (split == UINT32_MAX)
                return false;
            (*instAt(program, split)).a = split + 1u;
            if (!emitNode(program, nodes, RegexNode_a(node)))
                return false;
            u32 jump = emit(program, REGEX_OP_JUMP, 0, 0, 0);
            if (jump == UINT32_MAX)
                return false;
            (*instAt(program, jump)).a = split;
            (*instAt(program, split)).b = RegexProgram_count(program);
            return true;
        }
        for (u32 i = min; i < max; i++) {
            u32 split = emit(program, REGEX_OP_SPLIT, 0, 0, 0);
            if (split == UINT32_MAX)
                return false;
            (*instAt(program, split)).a = split + 1u;
            if (!emitNode(program, nodes, RegexNode_a(node)))
                return false;
            (*instAt(program, split)).b = RegexProgram_count(program);
        }
        return true;
    }
    case REGEX_NODE_GROUP: {
        u32 group = RegexNode_group(node);
        if (group > 0)
            if (emit(program, REGEX_OP_SAVE, 0, group * 2u, 0) == UINT32_MAX)
                return false;
        if (!emitNode(program, nodes, RegexNode_a(node)))
            return false;
        if (group > 0)
            if (emit(program, REGEX_OP_SAVE, 0, group * 2u + 1u, 0) == UINT32_MAX)
                return false;
        return true;
    }
    default:
        return false;
    }
}

RegexProgram RegexProgram_0() {
    RegexProgram program;
    program.insts = nullptr;
    program.count = 0u;
    program.capacity = 0u;
    return program;
}

bool RegexProgram_build(RegexProgram *program, const RegexNodeList *nodes, u32 root) {
    if (emit(program, REGEX_OP_SAVE, 0, 0, 0) == UINT32_MAX)
        return false;
    if (!emitNode(program, nodes, root))
        return false;
    if (emit(program, REGEX_OP_SAVE, 0, 1, 0) == UINT32_MAX)
        return false;
    if (emit(program, REGEX_OP_MATCH, 0, 0, 0) == UINT32_MAX)
        return false;
    return true;
}

u32 RegexProgram_count(const RegexProgram *program) {
    return RegexProgram_count(program);
}

const RegexInst *RegexProgram_at(const RegexProgram *program, u32 index) {
    if (index >= RegexProgram_count(program))
        return nullptr;
    return &RegexProgram_instsMut(program)[index];
}

fn RegexProgram_free(RegexProgram *program) {
    if (RegexProgram_insts(program))
        Memory_free(RegexProgram_insts(program));
    *program = RegexProgram();
}
