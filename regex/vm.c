// regex/vm.c — the RegexVm Pike-VM simulator.

#include "vex.h"
#include "regex/vm.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: RegexVm
 * ============================================================================
 * The matcher: a Pike-VM simulation of a compiled program over a subject. It
 * exists rather than a backtracking matcher because backtracking gives a
 * hostile pattern a way to hang its caller — the classic (a+)+b.sh blow-up — and
 * this engine is called from cold seams that must always return. The
 * simulation keeps every alternative alive at once as a thread list, advances
 * all of them one subject byte at a time. Capture-row copies also scale with
 * group count: O((length + 1) * instructions * (groups + 1)). Finite input
 * gives finite work, not a wall-clock deadline; execution budgets and
 * cancellation are still absent and this is not bounded-join worker proof.
 *
 * A thread is a program counter plus a capture row. The epsilon-closure (the
 * zero-width part: SPLIT, JUMP, SAVE, anchors) runs on an explicit work stack,
 * never the C stack, so a deep program cannot overflow; and a capture write is
 * paired with a restore marker popped after its subtree, so two branches of
 * one split never see each other's writes. Thread rows live in two per-list
 * regions that are fully rewritten every position, so memory is bounded and
 * nothing is carried by reference across a reallocation.
 *
 * memory: everything is allocated once in RegexVm(...) and freed in
 * RegexVm_free — a run allocates nothing (the hot path is a plain byte loop).
 * The subject is borrowed for the duration of the run and never retained.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: RegexVm (regex/vm.c — defined in regex/vm.h)
 * ============================================================================
 * Bounded, backtracking-free simulation of a RegexProgram over a byte subject.
 *
 * STRUCT FIELDS (Mirroring regex/vm.h):
 * ----------------------------------------------------------------------------
 *   RegexVm {
 *     const RegexProgram *program;  // borrowed instruction source
 *     u32 *clistPc;            // current thread list: program counters
 *     u32 *clistCap;           // current thread list: capture rows
 *     u32 *clistSeen;          // current list's per-position marks
 *     u32 *nlistPc;            // next thread list: program counters
 *     u32 *nlistCap;           // next thread list: capture rows
 *     u32 *nlistSeen;          // next list's per-position marks
 *     RegexTask *stack;             // epsilon-closure work stack
 *     bytes *caps;                 // two capture regions (2 x capRowCount)
 *     bytes *work;                 // the closure's working capture row
 *     bytes *seed;                 // the all-unset row every search starts from
 *     bytes *best;                 // the winning capture row
 *     u32 capStride;           // bytes slots per capture row
 *     u32 capRowCount;         // rows per region
 *     u32 stackCapacity;       // work-stack entries
 *   }
 *
 * PRIVATE HELPERS (documented here because they appear in this class layout or
 * in its threads):
 * ----------------------------------------------------------------------------
 *   RegexOptions { bool ignoreCase; bool dotall; bool multiline; }
 *     Resolved match-time options; declared in regex/vm.h.
 *   RegexTask { uint32 pc; uint32 slot; bytes value; uint8 restore; }
 *     One closure work item (expand a pc, or restore a capture slot).
 *   RegexList (file-local) { uint32 *pc; uint32 *cap; uint32 *seen;
 *                            uint32 count; uint32 capBase; }
 *     A Thompson thread list plus the capture region it writes into.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - RegexVm(program, groupCount)
 *   - RegexVm_free(vm)
 *   - RegexVm_run(vm, classes, subject, length, anchored, options)
 *   - RegexVm_runFull(vm, classes, subject, length, options)
 *   - RegexVm_span(vm, group)
 *
 * Private Core Functions: (.c static)
 *   - lowerByte(c) / capRowBase(list) / addThread(...) / instMatches(...)
 *   - run(..., full): full mode ignores premature MATCH without pruning peers.
 *
 * Admission, ownership, failure:
 *   - init sizes every block from the program and groupCount; false means the
 *     arena refused and the VM must not be run. run allocates nothing.
 *   - run borrows subject for its duration; a no-match return leaves best
 *     stale (callers read spans only after true).
 *   - free releases every block and zeroes the VM; null-safe.
 * ============================================================================
 */


;;GETTER
static inline u32 RegexInst_a(const RegexInst *s) { return (*s).a; }
;;GETTER
static inline u32 *RegexInst_aMut(RegexInst *s) { return &(*s).a; }
;;SETTER
static inline fn RegexInst_setA(RegexInst *s, u32 v) { (*s).a = v; }
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
static inline bytes * RegexVm_best(const RegexVm *s) { return (*s).best; }
;;GETTER
static inline bytes * *RegexVm_bestMut(RegexVm *s) { return &(*s).best; }
;;SETTER
static inline fn RegexVm_setBest(RegexVm *s, bytes * v) { (*s).best = v; }
;;GETTER
static inline u32 RegexVm_capRowCount(const RegexVm *s) { return (*s).capRowCount; }
;;GETTER
static inline u32 *RegexVm_capRowCountMut(RegexVm *s) { return &(*s).capRowCount; }
;;SETTER
static inline fn RegexVm_setCapRowCount(RegexVm *s, u32 v) { (*s).capRowCount = v; }
;;GETTER
static inline u32 RegexVm_capStride(const RegexVm *s) { return (*s).capStride; }
;;GETTER
static inline u32 *RegexVm_capStrideMut(RegexVm *s) { return &(*s).capStride; }
;;SETTER
static inline fn RegexVm_setCapStride(RegexVm *s, u32 v) { (*s).capStride = v; }
;;GETTER
static inline bytes * RegexVm_caps(const RegexVm *s) { return (*s).caps; }
;;GETTER
static inline bytes * *RegexVm_capsMut(RegexVm *s) { return &(*s).caps; }
;;SETTER
static inline fn RegexVm_setCaps(RegexVm *s, bytes * v) { (*s).caps = v; }
;;GETTER
static inline u32 * RegexVm_clistCap(const RegexVm *s) { return (*s).clistCap; }
;;GETTER
static inline u32 * *RegexVm_clistCapMut(RegexVm *s) { return &(*s).clistCap; }
;;SETTER
static inline fn RegexVm_setClistCap(RegexVm *s, u32 * v) { (*s).clistCap = v; }
;;GETTER
static inline u32 * RegexVm_clistPc(const RegexVm *s) { return (*s).clistPc; }
;;GETTER
static inline u32 * *RegexVm_clistPcMut(RegexVm *s) { return &(*s).clistPc; }
;;SETTER
static inline fn RegexVm_setClistPc(RegexVm *s, u32 * v) { (*s).clistPc = v; }
;;GETTER
static inline u32 * RegexVm_clistSeen(const RegexVm *s) { return (*s).clistSeen; }
;;GETTER
static inline u32 * *RegexVm_clistSeenMut(RegexVm *s) { return &(*s).clistSeen; }
;;SETTER
static inline fn RegexVm_setClistSeen(RegexVm *s, u32 * v) { (*s).clistSeen = v; }
;;GETTER
static inline u32 * RegexVm_nlistCap(const RegexVm *s) { return (*s).nlistCap; }
;;GETTER
static inline u32 * *RegexVm_nlistCapMut(RegexVm *s) { return &(*s).nlistCap; }
;;SETTER
static inline fn RegexVm_setNlistCap(RegexVm *s, u32 * v) { (*s).nlistCap = v; }
;;GETTER
static inline u32 * RegexVm_nlistPc(const RegexVm *s) { return (*s).nlistPc; }
;;GETTER
static inline u32 * *RegexVm_nlistPcMut(RegexVm *s) { return &(*s).nlistPc; }
;;SETTER
static inline fn RegexVm_setNlistPc(RegexVm *s, u32 * v) { (*s).nlistPc = v; }
;;GETTER
static inline u32 * RegexVm_nlistSeen(const RegexVm *s) { return (*s).nlistSeen; }
;;GETTER
static inline u32 * *RegexVm_nlistSeenMut(RegexVm *s) { return &(*s).nlistSeen; }
;;SETTER
static inline fn RegexVm_setNlistSeen(RegexVm *s, u32 * v) { (*s).nlistSeen = v; }
;;GETTER
static inline const RegexProgram * RegexVm_program(const RegexVm *s) { return (*s).program; }
;;GETTER
static inline const RegexProgram * *RegexVm_programMut(RegexVm *s) { return &(*s).program; }
;;SETTER
static inline fn RegexVm_setProgram(RegexVm *s, const RegexProgram * v) { (*s).program = v; }
;;GETTER
static inline bytes * RegexVm_seed(const RegexVm *s) { return (*s).seed; }
;;GETTER
static inline bytes * *RegexVm_seedMut(RegexVm *s) { return &(*s).seed; }
;;SETTER
static inline fn RegexVm_setSeed(RegexVm *s, bytes * v) { (*s).seed = v; }
;;GETTER
static inline RegexTask * RegexVm_stack(const RegexVm *s) { return (*s).stack; }
;;GETTER
static inline RegexTask * *RegexVm_stackMut(RegexVm *s) { return &(*s).stack; }
;;SETTER
static inline fn RegexVm_setStack(RegexVm *s, RegexTask * v) { (*s).stack = v; }
;;GETTER
static inline u32 RegexVm_stackCapacity(const RegexVm *s) { return (*s).stackCapacity; }
;;GETTER
static inline u32 *RegexVm_stackCapacityMut(RegexVm *s) { return &(*s).stackCapacity; }
;;SETTER
static inline fn RegexVm_setStackCapacity(RegexVm *s, u32 v) { (*s).stackCapacity = v; }
;;GETTER
static inline bytes * RegexVm_work(const RegexVm *s) { return (*s).work; }
;;GETTER
static inline bytes * *RegexVm_workMut(RegexVm *s) { return &(*s).work; }
;;SETTER
static inline fn RegexVm_setWork(RegexVm *s, bytes * v) { (*s).work = v; }

// ASCII-only lowercase; deliberately not locale-sensitive (byte subjects).
static u8 lowerByte(u8 c) {
    if (c >= (u8) 'A' && c <= (u8) 'Z')
        return (u8)(c + 32u);
    return c;
}

// One Thompson thread list: a program counter and capture row per thread, the
// per-position "already added" marks, and the half of the capture table this
// list writes its rows into.
typedef struct RegexList {
    u32 *pc;      // thread program counters (capRowCount entries)
    u32 *cap;     // thread capture rows
    u32 *seen;    // per-position dedup marks (program length entries)
    u32  count;   // threads in use
    u32  capBase; // first capture row this list owns
} RegexList;

;;GETTER
static inline u32 * RegexList_cap(const RegexList *s) { return (*s).cap; }
;;GETTER
static inline u32 * *RegexList_capMut(RegexList *s) { return &(*s).cap; }
;;SETTER
static inline fn RegexList_setCap(RegexList *s, u32 * v) { (*s).cap = v; }
;;GETTER
static inline u32 RegexList_capBase(const RegexList *s) { return (*s).capBase; }
;;GETTER
static inline u32 *RegexList_capBaseMut(RegexList *s) { return &(*s).capBase; }
;;SETTER
static inline fn RegexList_setCapBase(RegexList *s, u32 v) { (*s).capBase = v; }
;;GETTER
static inline u32 RegexList_count(const RegexList *s) { return (*s).count; }
;;GETTER
static inline u32 *RegexList_countMut(RegexList *s) { return &(*s).count; }
;;SETTER
static inline fn RegexList_setCount(RegexList *s, u32 v) { (*s).count = v; }
;;GETTER
static inline u32 * RegexList_pc(const RegexList *s) { return (*s).pc; }
;;GETTER
static inline u32 * *RegexList_pcMut(RegexList *s) { return &(*s).pc; }
;;SETTER
static inline fn RegexList_setPc(RegexList *s, u32 * v) { (*s).pc = v; }
;;GETTER
static inline u32 * RegexList_seen(const RegexList *s) { return (*s).seen; }
;;GETTER
static inline u32 * *RegexList_seenMut(RegexList *s) { return &(*s).seen; }
;;SETTER
static inline fn RegexList_setSeen(RegexList *s, u32 * v) { (*s).seen = v; }


// Adds a capture row from the head of this list's region.
static bytes *capRowBase(RegexVm *vm, RegexList *list, u32 index) {
    return RegexVm_caps(vm) + (bytes)(RegexList_capBase(list) + index) * RegexVm_capStride(vm);
}

// Does a consuming instruction accept this byte? (CHAR bytes and class bitmaps
// were already folded at compile time.)
static bool instMatches(const RegexCharClass *classes, const RegexInst *inst, u8 byte,
                        RegexOptions options) {
    switch (RegexInst_op(inst)) {
    case REGEX_OP_CHAR:
        return byte == RegexInst_byte(inst);
    case REGEX_OP_ANY:
        if (options.dotall)
            return true;
        return byte != (u8) '\n';
    case REGEX_OP_CLASS:
        return RegexCharClass_test(&classes[RegexInst_a(inst)], byte);
    default:
        return false;
    }
}

// Epsilon-closure: adds pc0 and everything reachable without consuming a byte,
// starting from parentCaps. Iterative (work stack), and a capture write is
// paired with a restore marker so sibling branches never see each other.
static fn addThread(RegexVm *vm, RegexList *list, u32 pc0, const bytes *parentCaps,
                      bytes pos, const char *subject, bytes length, bool multiline) {
    u32 stride = RegexVm_capStride(vm);
    u32 programCount = RegexProgram_count(RegexVm_program(vm));
    bytes *work = RegexVm_work(vm);
    RegexTask *stack = RegexVm_stack(vm);
    u32 sp = 0;
    memcpy(work, parentCaps, (bytes) stride * sizeof(bytes));
    stack[sp].pc = pc0;
    stack[sp].restore = 0;
    sp++;
    while (sp > 0) {
        sp--;
        RegexTask task = stack[sp];
        if (task.restore) {
            work[task.slot] = task.value;
            continue;
        }
        if (RegexList_seen(list)[task.pc])
            continue;
        RegexList_seen(list)[task.pc] = 1u;
        const RegexInst *raw = RegexProgram_at(RegexVm_program(vm), task.pc);
        if (!raw)
            continue;
        RegexInst inst = *raw;
        switch (inst.op) {
        case REGEX_OP_JUMP:
            stack[sp].pc = inst.a;
            stack[sp].restore = 0;
            sp++;
            break;
        case REGEX_OP_SPLIT:
            stack[sp].pc = inst.b;
            stack[sp].restore = 0;
            sp++;
            stack[sp].pc = inst.a;
            stack[sp].restore = 0;
            sp++;
            break;
        case REGEX_OP_SAVE: {
            // Restore marker first, then the continuation, then mutate.
            stack[sp].pc = 0;
            stack[sp].slot = inst.a;
            stack[sp].value = work[inst.a];
            stack[sp].restore = 1;
            sp++;
            stack[sp].pc = task.pc + 1u;
            stack[sp].restore = 0;
            sp++;
            work[inst.a] = pos;
            break;
        }
        case REGEX_OP_BOL:
            if (pos == 0 || (multiline && subject[pos - 1] == '\n')) {
                stack[sp].pc = task.pc + 1u;
                stack[sp].restore = 0;
                sp++;
            }
            break;
        case REGEX_OP_EOL:
            if (pos == length || (multiline && subject[pos] == '\n')) {
                stack[sp].pc = task.pc + 1u;
                stack[sp].restore = 0;
                sp++;
            }
            break;
        default: {
            // A consuming instruction (or MATCH) becomes a thread with a full
            // copy of the working captures, in this list's own region.
            u32 index = RegexList_count(list);
            if (index >= RegexVm_capRowCount(vm) || task.pc >= programCount)
                break; // unreachable: one row per thread slot
            memcpy(capRowBase(vm, list, index), work, (bytes) stride * sizeof(bytes));
            RegexList_pc(list)[index] = task.pc;
            RegexList_cap(list)[index] = RegexList_capBase(list) + index;
            RegexList_setCount(list, index + 1u);
            break;
        }
        }
    }
}

static bool vmInitialize(RegexVm *vm, const RegexProgram *program, u32 groupCount) {
    memset(vm, 0, sizeof(RegexVm));
    u32 n = RegexProgram_count(program);
    if (n == 0)
        return false;
    RegexVm_setProgram(vm, program);
    RegexVm_setCapStride(vm, 2u * (groupCount + 1u));
    RegexVm_setCapRowCount(vm, n);
    RegexVm_setStackCapacity(vm, 4u * n + 8u);
    RegexVm_setClistPc(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    RegexVm_setClistCap(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    RegexVm_setClistSeen(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    RegexVm_setNlistPc(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    RegexVm_setNlistCap(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    RegexVm_setNlistSeen(vm, (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32)));
    (*RegexVm_stackMut(vm)) = (RegexTask*) Memory_alloc(TYPE_REGEX,
                                            (bytes)RegexVm_stackCapacity(vm) * sizeof(RegexTask));
    (*RegexVm_capsMut(vm)) = (bytes*) Memory_alloc(TYPE_REGEX,
                                        2u * (bytes) n * RegexVm_capStride(vm) * sizeof(bytes));
    RegexVm_setWork(vm, (bytes*) Memory_alloc(TYPE_REGEX, (bytes)RegexVm_capStride(vm) * sizeof(bytes)));
    RegexVm_setSeed(vm, (bytes*) Memory_alloc(TYPE_REGEX, (bytes)RegexVm_capStride(vm) * sizeof(bytes)));
    RegexVm_setBest(vm, (bytes*) Memory_alloc(TYPE_REGEX, (bytes)RegexVm_capStride(vm) * sizeof(bytes)));
    if (!RegexVm_clistPc(vm) || !RegexVm_clistCap(vm) || !RegexVm_clistSeen(vm) || !RegexVm_nlistPc(vm) ||
        !RegexVm_nlistCap(vm) || !RegexVm_nlistSeen(vm) || !RegexVm_stack(vm) || !RegexVm_caps(vm) ||
        !RegexVm_work(vm) || !RegexVm_seed(vm) || !RegexVm_best(vm)) {
        RegexVm_free(vm);
        return false;
    }
    // Every capture slot starts as REGEX_UNSET (all bits set).
    memset(RegexVm_caps(vm), 0xFF, 2u * (bytes) n * RegexVm_capStride(vm) * sizeof(bytes));
    memset(RegexVm_seed(vm), 0xFF, (bytes)RegexVm_capStride(vm) * sizeof(bytes));
    memset(RegexVm_best(vm), 0xFF, (bytes)RegexVm_capStride(vm) * sizeof(bytes));
    return true;
}

RegexVm RegexVm_2(const RegexProgram *program, u32 groupCount) {
    RegexVm vm;
    memset(&vm, 0, sizeof vm);
    if (!vmInitialize(&vm, program, groupCount))
        RegexVm_free(&vm);
    return vm;
}

fn RegexVm_free(RegexVm *vm) {
    if (RegexVm_clistPc(vm)) Memory_free(RegexVm_clistPc(vm));
    if (RegexVm_clistCap(vm)) Memory_free(RegexVm_clistCap(vm));
    if (RegexVm_clistSeen(vm)) Memory_free(RegexVm_clistSeen(vm));
    if (RegexVm_nlistPc(vm)) Memory_free(RegexVm_nlistPc(vm));
    if (RegexVm_nlistCap(vm)) Memory_free(RegexVm_nlistCap(vm));
    if (RegexVm_nlistSeen(vm)) Memory_free(RegexVm_nlistSeen(vm));
    if (RegexVm_stack(vm)) Memory_free(RegexVm_stack(vm));
    if (RegexVm_caps(vm)) Memory_free(RegexVm_caps(vm));
    if (RegexVm_work(vm)) Memory_free(RegexVm_work(vm));
    if (RegexVm_seed(vm)) Memory_free(RegexVm_seed(vm));
    if (RegexVm_best(vm)) Memory_free(RegexVm_best(vm));
    memset(vm, 0, sizeof(RegexVm));
}

static bool run(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                bytes length, bool anchored, RegexOptions options, bool full) {
    u32 n = RegexVm_capRowCount(vm);
    RegexList clist;
    clist.pc = RegexVm_clistPc(vm);
    clist.cap = RegexVm_clistCap(vm);
    clist.seen = RegexVm_clistSeen(vm);
    clist.count = 0;
    clist.capBase = 0;
    RegexList nlist;
    nlist.pc = RegexVm_nlistPc(vm);
    nlist.cap = RegexVm_nlistCap(vm);
    nlist.seen = RegexVm_nlistSeen(vm);
    nlist.count = 0;
    nlist.capBase = n;
    memset(clist.seen, 0, (bytes) n * sizeof(u32));
    memset(nlist.seen, 0, (bytes) n * sizeof(u32));

    bool matched = false;
    for (bytes pos = 0;; pos++) {
        if (clist.count == 0 && matched)
            break;
        if (!matched && (!anchored || pos == 0) && pos <= length)
            addThread(vm, &clist, 0, RegexVm_seed(vm), pos, subject, length, options.multiline);
        nlist.count = 0;
        memset(nlist.seen, 0, (bytes) n * sizeof(u32));
        for (u32 i = 0; i < clist.count; i++) {
            u32 pc = clist.pc[i];
            const RegexInst *raw = RegexProgram_at(RegexVm_program(vm), pc);
            if (!raw)
                continue;
            if (RegexInst_op(raw) == REGEX_OP_MATCH) {
                if (full && pos != length)
                    continue;
                matched = true;
                memcpy(RegexVm_best(vm), RegexVm_caps(vm) + (bytes) clist.cap[i] * RegexVm_capStride(vm),
                       (bytes)RegexVm_capStride(vm) * sizeof(bytes));
                break;
            }
            if (pos < length) {
                u8 byte = (u8) subject[pos];
                u8 folded = options.ignoreCase ? lowerByte(byte) : byte;
                if (instMatches(classes, raw, folded, options))
                    addThread(vm, &nlist, pc + 1u,
                              RegexVm_caps(vm) + (bytes) clist.cap[i] * RegexVm_capStride(vm),
                              pos + 1u, subject, length, options.multiline);
            }
        }
        RegexList swap = clist;
        clist = nlist;
        nlist = swap;
        if (pos >= length)
            break;
    }
    return matched;
}

bool RegexVm_run(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                 bytes length, bool anchored, RegexOptions options) {
    return run(vm, classes, subject, length, anchored, options, false);
}

bool RegexVm_runFull(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                     bytes length, RegexOptions options) {
    return run(vm, classes, subject, length, true, options, true);
}

RegexSpan RegexVm_span(const RegexVm *vm, u32 group) {
    bytes slot = 2u * (bytes) group;
    if (slot + 1u >= (bytes)RegexVm_capStride(vm))
        return RegexSpan_none();
    RegexSpan span;
    span.start = RegexVm_best(vm)[slot];
    span.end = RegexVm_best(vm)[slot + 1u];
    if (!RegexSpan_isSet(span))
        return RegexSpan_none();
    return span;
}
