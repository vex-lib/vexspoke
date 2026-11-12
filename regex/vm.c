// regex/vm.c — the RegexVm Pike-VM simulator.

#include "regex/vm.h"

#include <string.h>

#include "util/annotation.h"
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

// Adds a capture row from the head of this list's region.
static bytes *capRowBase(RegexVm *vm, RegexList *list, u32 index) {
    return (*vm).caps + (bytes)((*list).capBase + index) * (*vm).capStride;
}

// Does a consuming instruction accept this byte? (CHAR bytes and class bitmaps
// were already folded at compile time.)
static bool instMatches(const RegexCharClass *classes, const RegexInst *inst, u8 byte,
                        RegexOptions options) {
    switch ((*inst).op) {
    case REGEX_OP_CHAR:
        return byte == (*inst).byte;
    case REGEX_OP_ANY:
        if (options.dotall)
            return true;
        return byte != (u8) '\n';
    case REGEX_OP_CLASS:
        return RegexCharClass_test(&classes[(*inst).a], byte);
    default:
        return false;
    }
}

// Epsilon-closure: adds pc0 and everything reachable without consuming a byte,
// starting from parentCaps. Iterative (work stack), and a capture write is
// paired with a restore marker so sibling branches never see each other.
static void addThread(RegexVm *vm, RegexList *list, u32 pc0, const bytes *parentCaps,
                      bytes pos, const char *subject, bytes length, bool multiline) {
    u32 stride = (*vm).capStride;
    u32 programCount = RegexProgram_count((*vm).program);
    bytes *work = (*vm).work;
    RegexTask *stack = (*vm).stack;
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
        if ((*list).seen[task.pc])
            continue;
        (*list).seen[task.pc] = 1u;
        const RegexInst *raw = RegexProgram_at((*vm).program, task.pc);
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
            u32 index = (*list).count;
            if (index >= (*vm).capRowCount || task.pc >= programCount)
                break; // unreachable: one row per thread slot
            memcpy(capRowBase(vm, list, index), work, (bytes) stride * sizeof(bytes));
            (*list).pc[index] = task.pc;
            (*list).cap[index] = (*list).capBase + index;
            (*list).count = index + 1u;
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
    (*vm).program = program;
    (*vm).capStride = 2u * (groupCount + 1u);
    (*vm).capRowCount = n;
    (*vm).stackCapacity = 4u * n + 8u;
    (*vm).clistPc = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).clistCap = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).clistSeen = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).nlistPc = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).nlistCap = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).nlistSeen = (u32*) Memory_alloc(TYPE_REGEX, (bytes) n * sizeof(u32));
    (*vm).stack = (RegexTask*) Memory_alloc(TYPE_REGEX,
                                            (bytes)(*vm).stackCapacity * sizeof(RegexTask));
    (*vm).caps = (bytes*) Memory_alloc(TYPE_REGEX,
                                        2u * (bytes) n * (*vm).capStride * sizeof(bytes));
    (*vm).work = (bytes*) Memory_alloc(TYPE_REGEX, (bytes)(*vm).capStride * sizeof(bytes));
    (*vm).seed = (bytes*) Memory_alloc(TYPE_REGEX, (bytes)(*vm).capStride * sizeof(bytes));
    (*vm).best = (bytes*) Memory_alloc(TYPE_REGEX, (bytes)(*vm).capStride * sizeof(bytes));
    if (!(*vm).clistPc || !(*vm).clistCap || !(*vm).clistSeen || !(*vm).nlistPc ||
        !(*vm).nlistCap || !(*vm).nlistSeen || !(*vm).stack || !(*vm).caps ||
        !(*vm).work || !(*vm).seed || !(*vm).best) {
        RegexVm_free(vm);
        return false;
    }
    // Every capture slot starts as REGEX_UNSET (all bits set).
    memset((*vm).caps, 0xFF, 2u * (bytes) n * (*vm).capStride * sizeof(bytes));
    memset((*vm).seed, 0xFF, (bytes)(*vm).capStride * sizeof(bytes));
    memset((*vm).best, 0xFF, (bytes)(*vm).capStride * sizeof(bytes));
    return true;
}

RegexVm RegexVm_2(const RegexProgram *program, u32 groupCount) {
    RegexVm vm;
    memset(&vm, 0, sizeof vm);
    if (!vmInitialize(&vm, program, groupCount))
        RegexVm_free(&vm);
    return vm;
}

void RegexVm_free(RegexVm *vm) {
    if ((*vm).clistPc) Memory_free((*vm).clistPc);
    if ((*vm).clistCap) Memory_free((*vm).clistCap);
    if ((*vm).clistSeen) Memory_free((*vm).clistSeen);
    if ((*vm).nlistPc) Memory_free((*vm).nlistPc);
    if ((*vm).nlistCap) Memory_free((*vm).nlistCap);
    if ((*vm).nlistSeen) Memory_free((*vm).nlistSeen);
    if ((*vm).stack) Memory_free((*vm).stack);
    if ((*vm).caps) Memory_free((*vm).caps);
    if ((*vm).work) Memory_free((*vm).work);
    if ((*vm).seed) Memory_free((*vm).seed);
    if ((*vm).best) Memory_free((*vm).best);
    memset(vm, 0, sizeof(RegexVm));
}

static bool run(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                bytes length, bool anchored, RegexOptions options, bool full) {
    u32 n = (*vm).capRowCount;
    RegexList clist;
    clist.pc = (*vm).clistPc;
    clist.cap = (*vm).clistCap;
    clist.seen = (*vm).clistSeen;
    clist.count = 0;
    clist.capBase = 0;
    RegexList nlist;
    nlist.pc = (*vm).nlistPc;
    nlist.cap = (*vm).nlistCap;
    nlist.seen = (*vm).nlistSeen;
    nlist.count = 0;
    nlist.capBase = n;
    memset(clist.seen, 0, (bytes) n * sizeof(u32));
    memset(nlist.seen, 0, (bytes) n * sizeof(u32));

    bool matched = false;
    for (bytes pos = 0;; pos++) {
        if (clist.count == 0 && matched)
            break;
        if (!matched && (!anchored || pos == 0) && pos <= length)
            addThread(vm, &clist, 0, (*vm).seed, pos, subject, length, options.multiline);
        nlist.count = 0;
        memset(nlist.seen, 0, (bytes) n * sizeof(u32));
        for (u32 i = 0; i < clist.count; i++) {
            u32 pc = clist.pc[i];
            const RegexInst *raw = RegexProgram_at((*vm).program, pc);
            if (!raw)
                continue;
            if ((*raw).op == REGEX_OP_MATCH) {
                if (full && pos != length)
                    continue;
                matched = true;
                memcpy((*vm).best, (*vm).caps + (bytes) clist.cap[i] * (*vm).capStride,
                       (bytes)(*vm).capStride * sizeof(bytes));
                break;
            }
            if (pos < length) {
                u8 byte = (u8) subject[pos];
                u8 folded = options.ignoreCase ? lowerByte(byte) : byte;
                if (instMatches(classes, raw, folded, options))
                    addThread(vm, &nlist, pc + 1u,
                              (*vm).caps + (bytes) clist.cap[i] * (*vm).capStride,
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
    if (slot + 1u >= (bytes)(*vm).capStride)
        return RegexSpan_none();
    RegexSpan span;
    span.start = (*vm).best[slot];
    span.end = (*vm).best[slot + 1u];
    if (!RegexSpan_isSet(span))
        return RegexSpan_none();
    return span;
}
