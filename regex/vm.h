#ifndef REGEX_VM_H
#define REGEX_VM_H

#include "vex.h"
#include "regex/charclass.h"
#include "regex/program.h"
#include "regex/span.h"

// regex/vm.h — the RegexVm: the Pike-VM simulator that executes a compiled
// program over a subject. Single Class Per File Law: RegexVm.
//
// For a fixed compiled pattern, work scales linearly with subject length:
// O((length + 1) * instructions * (groups + 1)), including capture copies.
// This is not a wall-clock bound or cancellation guarantee. NO backtracking
// and NO recursion occur during matching, and
// everything it needs is allocated once, before the run. A thread list carries
// the still-live NFA states; each capture set lives in a row of the capture
// table; and the epsilon-closure walks a work stack instead of the C stack.
// The winning capture row is copied out the moment MATCH is reached.

// The match-time options, resolved from the public REGEX_* flag word by
// regex/regex.c so this unit never depends on the public header.
typedef struct RegexOptions {
    bool ignoreCase;  // fold the subject byte when comparing REGEX_OP_CHAR
    bool dotall;      // REGEX_OP_ANY also consumes newline
    bool multiline;   // '^'/'\$' also match at line edges
} RegexOptions;

static inline RegexOptions RegexOptions_3(bool ignoreCase, bool dotall, bool multiline) {
    return (RegexOptions) { .ignoreCase = ignoreCase, .dotall = dotall, .multiline = multiline };
}
#define RegexOptions(...) CONSTRUCTOR_DISPATCH(RegexOptions, __VA_ARGS__)
static inline fn RegexOptions_setIgnoreCase(RegexOptions *options, bool value) {
    if (options) (*options).ignoreCase = value;
}
static inline fn RegexOptions_setDotall(RegexOptions *options, bool value) {
    if (options) (*options).dotall = value;
}
static inline fn RegexOptions_setMultiline(RegexOptions *options, bool value) {
    if (options) (*options).multiline = value;
}

// One closure work item: either "expand this pc" or "restore a capture slot
// after the subtree beneath it finishes" (so sibling branches stay clean).
typedef struct RegexTask {
    u32 pc;       // program counter to expand (restore items: 0)
    u32 slot;     // capture slot to restore
    bytes   value;    // previous value of that slot
    u8  restore;  // 1 = this is a restore marker
} RegexTask;

// Per-run scratch, sized once from the program. One row per thread slot in
// each of two capture regions, a shared all-unset seed row, and the copied
// winner. Immutable except for `best` after construction.
typedef struct RegexVm {
    const RegexProgram *program;       // borrowed instruction source
    u32   *clistPc;               // current thread list: program counters
    u32   *clistCap;              // current thread list: capture rows
    u32   *clistSeen;             // current list's per-position marks
    u32   *nlistPc;               // next thread list: program counters
    u32   *nlistCap;              // next thread list: capture rows
    u32   *nlistSeen;             // next list's per-position marks
    RegexTask  *stack;                 // epsilon-closure work stack
    bytes     *caps;                  // two capture regions (2 x capRowCount)
    bytes     *work;                  // the closure's working capture row
    bytes     *seed;                  // the all-unset row every search starts from
    bytes     *best;                  // the winning capture row
    u32    capStride;             // bytes slots per capture row
    u32    capRowCount;           // rows per region
    u32    stackCapacity;         // work-stack entries
} RegexVm;

// Allocate every scratch block up front; a run allocates nothing more. A
// refused allocation returns an empty VM (`best == nullptr`).
RegexVm RegexVm_2(const RegexProgram *program, u32 groupCount);
#define RegexVm(...) CONSTRUCTOR_DISPATCH(RegexVm, __VA_ARGS__)

// Release every scratch block and zero the VM. Null-safe.
fn RegexVm_free(RegexVm *vm);

// Run the simulation. anchored restricts the match to offset 0. Returns true
// on a match and copies the winning capture row into the VM's best row.
bool RegexVm_run(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                 bytes length, bool anchored, RegexOptions options);

// Accept only at subject end, starting at zero. A premature MATCH does not
// discard competing alternatives; search branch priority otherwise survives.
bool RegexVm_runFull(RegexVm *vm, const RegexCharClass *classes, const char *subject,
                     bytes length, RegexOptions options);

// The winning capture span for one group (0 = the whole match); an unmatched
// group yields RegexSpan_none(). Valid after a successful run.
RegexSpan RegexVm_span(const RegexVm *vm, u32 group);

#endif
