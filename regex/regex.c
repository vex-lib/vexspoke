// regex/regex.c — the public Regex class: compile once, match in linear time.

#include "vex.h"
#include <stdarg.h>

#include "regex/regex.h"

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "regex/parser.h"
#include "regex/vm.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Regex
 * ============================================================================
 * The public face of the regex subsystem: compile a pattern once, then answer
 * "does this text look like that shape?" in time bounded by the input. It
 * exists because the ecosystem keeps needing text shapes — find/replace in
 * editors, log and manifest scanning, connector input validation, syntax
 * tooling — and neither a hand-rolled strstr nor a backtracking matcher is an
 * acceptable answer: the first cannot express the shape, the second hands a
 * hostile pattern a way to hang its caller.
 *
 * This unit owns only the orchestration and the public contract. The work
 * lives in one unit per concern: regex/parser.c turns pattern bytes into a
 * flat AST plus class bitmaps, regex/program.c lowers that AST into a Thompson
 * NFA, and regex/vm.c simulates it. Compiling is parse -> emit -> keep the
 * classes -> drop the AST; matching builds a fresh VM, runs it, reads spans and
 * frees it, so a compiled Regex is immutable and shareable while two threads
 * never share mutable state.
 *
 * memory: the Regex block, its pattern copy, its RegexProgram and its class
 * bitmap array all live in the engine arena and are stamped TYPE_REGEX. Each
 * match call allocates its own bounded scratch (in regex/vm.c) and releases it
 * on every path, including the no-match return.
 *
 * Failure: a null pattern, an unsupported escape or group, an unbalanced
 * bracket, an invalid range, a malformed quantifier or any safety-ceiling
 * breach is a loud cold rejection (THROW) returning nullptr — nothing
 * half-compiled escapes, because a half-compiled pattern would match the wrong
 * language (the Cold-Strict, Hot-Minimal Validation Law).
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Regex (regex/regex.c — defined in regex/regex.h)
 * ============================================================================
 * A compiled, byte-oriented regular expression: the public compile entry, the
 * public match surface, and the two string projections.
 *
 * STRUCT FIELDS (Mirroring regex/regex.h):
 * ----------------------------------------------------------------------------
 *   Regex {
 *     char           *pattern;     // owned copy of the source pattern text
 *     u32        flags;       // REGEX_* options used at compile time
 *     u32        groupCount;  // capturing groups (group 0 is the whole match)
 *     RegexProgram    program;     // the compiled instruction stream
 *     RegexCharClass *classes;     // classCount bitmaps (engine-arena block)
 *     u32        classCount;  // class bitmaps in use
 *   }
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   RegexText { char *dest; bytes cap; bytes used; bool truncated; }
 *     Bounded-append cursor for the two string projections.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - Regex_1(pattern) / Regex_2(pattern, flags) / Regex(...) / Regex_free(regex)
 *   - Regex_pattern / Regex_flags / Regex_groupCount / Regex_instCount /
 *     Regex_classCount
 *   - Regex_isMatch / Regex_isFullMatch / Regex_find / Regex_findGroups
 *   - Regex_toString / Regex_toStringStruct
 *
 * Private Core Functions: (.c static)
 *   - regexOptions(flags) / compilePattern(regex, pattern, length)
 *   - textAppend(text, s) / textPrintf(text, fmt, ...) / copyText(s, dest, cap, flag)
 *
 * Admission, ownership, failure:
 *   - Regex_1/Regex_2 admit a non-null NUL-terminated pattern; every rejection
 *     is loud (THROW) and returns nullptr with nothing partially published.
 *   - Regex_find / Regex_findGroups / Regex_isMatch / Regex_isFullMatch write
 *     outputs only on a match; on no match they return false with outputs
 *     untouched.
 *   - Regex_free is null-safe and releases pattern, program and classes.
 *   - Matching never allocates outside the VM's own scratch and always frees
 *     it, including on a no-match return.
 * ============================================================================
 */


;;GETTER
static inline bytes RegexSpan_end(const RegexSpan *s) { return (*s).end; }
;;GETTER
static inline bytes *RegexSpan_endMut(RegexSpan *s) { return &(*s).end; }
;;SETTER
static inline fn RegexSpan_setEnd(RegexSpan *s, bytes v) { (*s).end = v; }
;;GETTER
static inline bytes RegexSpan_start(const RegexSpan *s) { return (*s).start; }
;;GETTER
static inline bytes *RegexSpan_startMut(RegexSpan *s) { return &(*s).start; }
;;SETTER
static inline fn RegexSpan_setStart(RegexSpan *s, bytes v) { (*s).start = v; }

;;GETTER
static inline u32 Regex_classCount(const Regex *s) { return (*s).classCount; }
;;GETTER
static inline u32 *Regex_classCountMut(Regex *s) { return &(*s).classCount; }
;;SETTER
static inline fn Regex_setClassCount(Regex *s, u32 v) { (*s).classCount = v; }
;;GETTER
static inline RegexCharClass * Regex_classes(const Regex *s) { return (*s).classes; }
;;GETTER
static inline RegexCharClass * *Regex_classesMut(Regex *s) { return &(*s).classes; }
;;SETTER
static inline fn Regex_setClasses(Regex *s, RegexCharClass * v) { (*s).classes = v; }
;;GETTER
static inline u32 Regex_flags(const Regex *s) { return (*s).flags; }
;;GETTER
static inline u32 *Regex_flagsMut(Regex *s) { return &(*s).flags; }
;;SETTER
static inline fn Regex_setFlags(Regex *s, u32 v) { (*s).flags = v; }
;;GETTER
static inline u32 Regex_groupCount(const Regex *s) { return (*s).groupCount; }
;;GETTER
static inline u32 *Regex_groupCountMut(Regex *s) { return &(*s).groupCount; }
;;SETTER
static inline fn Regex_setGroupCount(Regex *s, u32 v) { (*s).groupCount = v; }
;;GETTER
static inline char * Regex_pattern(const Regex *s) { return (*s).pattern; }
;;GETTER
static inline char * *Regex_patternMut(Regex *s) { return &(*s).pattern; }
;;SETTER
static inline fn Regex_setPattern(Regex *s, char * v) { (*s).pattern = v; }
;;GETTER
static inline RegexProgram Regex_program(const Regex *s) { return (*s).program; }
;;GETTER
static inline RegexProgram *Regex_programMut(Regex *s) { return &(*s).program; }
;;SETTER
static inline fn Regex_setProgram(Regex *s, RegexProgram v) { (*s).program = v; }

typedef struct RegexText {
    char  *dest;       // caller buffer
    bytes cap;        // caller capacity (always > 0 here)
    bytes used;       // bytes written, excluding the NUL
    bool   truncated;  // true once a write did not fit
} RegexText;

;;GETTER
static inline bytes RegexText_cap(const RegexText *s) { return (*s).cap; }
;;GETTER
static inline bytes *RegexText_capMut(RegexText *s) { return &(*s).cap; }
;;SETTER
static inline fn RegexText_setCap(RegexText *s, bytes v) { (*s).cap = v; }
;;GETTER
static inline char * RegexText_dest(const RegexText *s) { return (*s).dest; }
;;GETTER
static inline char * *RegexText_destMut(RegexText *s) { return &(*s).dest; }
;;SETTER
static inline fn RegexText_setDest(RegexText *s, char * v) { (*s).dest = v; }
;;GETTER
static inline bool RegexText_truncated(const RegexText *s) { return (*s).truncated; }
;;GETTER
static inline bool *RegexText_truncatedMut(RegexText *s) { return &(*s).truncated; }
;;SETTER
static inline fn RegexText_setTruncated(RegexText *s, bool v) { (*s).truncated = v; }
;;GETTER
static inline bytes RegexText_used(const RegexText *s) { return (*s).used; }
;;GETTER
static inline bytes *RegexText_usedMut(RegexText *s) { return &(*s).used; }
;;SETTER
static inline fn RegexText_setUsed(RegexText *s, bytes v) { (*s).used = v; }


// Maps the public flag word onto the VM's resolved options.
static RegexOptions regexOptions(u32 flags) {
    RegexOptions options;
    options.ignoreCase = (flags & REGEX_CASE_INSENSITIVE) != 0;
    options.dotall = (flags & REGEX_DOTALL) != 0;
    options.multiline = (flags & REGEX_MULTILINE) != 0;
    return options;
}

// Runs one anchored or unanchored search and, on success, reports spans.
static bool runSearch(const Regex *regex, const char *subject, bytes length,
                      bool anchored, RegexSpan *outSpans, bytes spanCapacity,
                      bytes *outGroupCount, bool *outTruncated) {
    if (!regex || !subject)
        return false;
    RegexVm vm = RegexVm(Regex_programMut(regex), Regex_groupCount(regex));
    if (vm.best == nullptr)
        return false;
    RegexOptions options = regexOptions(Regex_flags(regex));
    bool hit = anchored
        ? RegexVm_runFull(&vm, Regex_classes(regex), subject, length, options)
        : RegexVm_run(&vm, Regex_classes(regex), subject, length, false, options);
    if (hit && outSpans) {
        bytes groups = (bytes)Regex_groupCount(regex) + 1u;
        bytes fill = groups < spanCapacity ? groups : spanCapacity;
        for (bytes g = 0; g < fill; g++)
            outSpans[g] = RegexVm_span(&vm, (u32) g);
        if (outTruncated && groups > spanCapacity)
            *outTruncated = true;
    }
    if (hit && outGroupCount)
        *outGroupCount = (bytes)Regex_groupCount(regex) + 1u;
    RegexVm_free(&vm);
    return hit;
}

// Appends a NUL-terminated string, flagging a truncation instead of overrunning.
static fn textAppend(RegexText *text, const char *s) {
    bytes len = strlen(s);
    if (RegexText_used(text) + len >= RegexText_cap(text)) {
        bytes room = RegexText_cap(text) > RegexText_used(text) + 1u ? RegexText_cap(text) - RegexText_used(text) - 1u : 0;
        if (room > 0)
            memcpy(RegexText_dest(text) + RegexText_used(text), s, room);
        RegexText_setUsed(text, RegexText_used(text) + (room));
        RegexText_dest(text)[RegexText_used(text)] = '\0';
        RegexText_setTruncated(text, true);
        return;
    }
    memcpy(RegexText_dest(text) + RegexText_used(text), s, len);
    RegexText_setUsed(text, RegexText_used(text) + (len));
    RegexText_dest(text)[RegexText_used(text)] = '\0';
}

static fn textPrintf(RegexText *text, const char *fmt, ...) {
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    i32 n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (n < 0)
        return;
    textAppend(text, buffer);
}

// Writes a fixed string into a caller buffer with truncation reporting.
static bool copyText(const char *text, char *dest, bytes cap, bool *outTruncated) {
    if (!dest || cap == 0)
        return false;
    bytes len = strlen(text);
    bytes room = cap - 1u;
    if (len > room) {
        memcpy(dest, text, room);
        dest[room] = '\0';
        if (outTruncated)
            *outTruncated = true;
        return false;
    }
    memcpy(dest, text, len);
    dest[len] = '\0';
    if (outTruncated)
        *outTruncated = false;
    return true;
}

// Parse, emit and adopt for one already-allocated Regex.
static bool compilePattern(Regex *regex, const char *pattern, bytes length) {
    RegexParser parser = RegexParser(pattern, length,
                                     (Regex_flags(regex) & REGEX_CASE_INSENSITIVE) != 0);
    u32 root = RegexParser_parse(&parser);
    bool ok = root != REGEX_NODE_NONE && RegexParser_ok(&parser);
    if (ok) {
        Regex_setGroupCount(regex, RegexParser_groupCount(&parser));
        ok = RegexProgram_build(Regex_programMut(regex), RegexParser_nodes(&parser), root);
    }
    if (ok) {
        u32 classCount = 0;
        RegexCharClass *classes = RegexParser_releaseClasses(&parser, &classCount);
        Regex_setClasses(regex, classes);
        Regex_setClassCount(regex, classCount);
    }
    const char *error = RegexParser_error(&parser);
    RegexParser_free(&parser);
    if (!ok && error)
        THROW("regex compile: %s: %s", pattern, error);
    else if (!ok)
        THROW("regex compile: %s: program too large (program ceiling)", pattern);
    return ok;
}

Regex *Regex_2(const char *pattern, u32 flags) {
    if (!pattern) {
        THROW("regex compile: null pattern");
        return nullptr;
    }
    Regex *regex = (Regex*) Memory_alloc(TYPE_REGEX, sizeof(Regex));
    if (!regex) {
        THROW("regex compile: out of memory");
        return nullptr;
    }
    memset(regex, 0, sizeof(Regex));
    Regex_setProgram(regex, RegexProgram());
    Regex_setFlags(regex, flags);

    bytes length = strlen(pattern);
    Regex_setPattern(regex, (char*) Memory_alloc(TYPE_REGEX, length + 1u));
    if (!Regex_pattern(regex)) {
        THROW("regex compile: out of memory");
        Memory_free(regex);
        return nullptr;
    }
    memcpy(Regex_pattern(regex), pattern, length + 1u);

    if (!compilePattern(regex, pattern, length)) {
        Regex_free(regex);
        return nullptr;
    }
    return regex;
}

Regex *Regex_1(const char *pattern) {
    return Regex_2(pattern, REGEX_DEFAULT);
}

fn Regex_free(Regex *regex) {
    if (!regex)
        return;
    RegexProgram_free(Regex_programMut(regex));
    if (Regex_classes(regex))
        Memory_free(Regex_classes(regex));
    if (Regex_pattern(regex))
        Memory_free(Regex_pattern(regex));
    Memory_free(regex);
}

const char *Regex_pattern(const Regex *regex) {
    if (!regex)
        return "";
    return Regex_pattern(regex);
}

u32 Regex_flags(const Regex *regex) {
    if (!regex)
        return 0;
    return Regex_flags(regex);
}

u32 Regex_groupCount(const Regex *regex) {
    if (!regex)
        return 0;
    return Regex_groupCount(regex);
}

u32 Regex_instCount(const Regex *regex) {
    if (!regex)
        return 0;
    return RegexProgram_count(Regex_programMut(regex));
}

u32 Regex_classCount(const Regex *regex) {
    if (!regex)
        return 0;
    return Regex_classCount(regex);
}

bool Regex_isMatch(const Regex *regex, const char *subject) {
    if (!regex || !subject)
        return false;
    RegexSpan span;
    return runSearch(regex, subject, strlen(subject), false, &span, 1u, nullptr, nullptr);
}

bool Regex_isFullMatch(const Regex *regex, const char *subject) {
    if (!regex || !subject)
        return false;
    bytes length = strlen(subject);
    RegexSpan whole = RegexSpan_none();
    if (!runSearch(regex, subject, length, true, &whole, 1u, nullptr, nullptr))
        return false;
    return whole.start == 0 && whole.end == length;
}

bool Regex_find(const Regex *regex, const char *subject, bytes length, RegexSpan *outMatch) {
    if (!outMatch || !regex || !subject)
        return false;
    RegexSpan span = RegexSpan_none();
    if (!runSearch(regex, subject, length, false, &span, 1u, nullptr, nullptr))
        return false;
    RegexSpan_setStart(outMatch, span.start);
    RegexSpan_setEnd(outMatch, span.end);
    return true;
}

bool Regex_findGroups(const Regex *regex, const char *subject, bytes length,
                      RegexSpan *outSpans, bytes spanCapacity, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!regex || !subject || !outSpans)
        return false;
    return runSearch(regex, subject, length, false, outSpans, spanCapacity, nullptr, outTruncated);
}

bool Regex_toString(const Regex *regex, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!dest || cap == 0)
        return false;
    if (!regex)
        return copyText("nullptr", dest, cap, outTruncated);
    RegexText text;
    text.dest = dest;
    text.cap = cap;
    text.used = 0;
    text.truncated = false;
    dest[0] = '\0';
    textAppend(&text, "/");
    textAppend(&text, Regex_pattern(regex));
    textAppend(&text, "/");
    if (Regex_flags(regex) & REGEX_CASE_INSENSITIVE) textAppend(&text, "i");
    if (Regex_flags(regex) & REGEX_DOTALL) textAppend(&text, "s");
    if (Regex_flags(regex) & REGEX_MULTILINE) textAppend(&text, "m");
    textPrintf(&text, " (groups=%u, insts=%u)", Regex_groupCount(regex), Regex_instCount(regex));
    if (outTruncated)
        *outTruncated = text.truncated;
    return !text.truncated;
}

bool Regex_toStringStruct(const Regex *regex, char *dest, bytes cap, bool *outTruncated) {
    if (outTruncated)
        *outTruncated = false;
    if (!dest || cap == 0)
        return false;
    if (!regex)
        return copyText("nullptr", dest, cap, outTruncated);
    RegexText text;
    text.dest = dest;
    text.cap = cap;
    text.used = 0;
    text.truncated = false;
    dest[0] = '\0';
    textAppend(&text, "Regex { pattern=\"");
    textAppend(&text, Regex_pattern(regex));
    textAppend(&text, "\", flags=");
    textPrintf(&text, "%u", Regex_flags(regex));
    textPrintf(&text, ", groupCount=%u", Regex_groupCount(regex));
    textPrintf(&text, ", instCount=%u", Regex_instCount(regex));
    textPrintf(&text, ", classCount=%u", Regex_classCount(regex));
    textAppend(&text, ", program=");
    RegexProgram program = Regex_program(regex);
    textPrintf(&text, "%s", program.insts ? "set" : "null");
    textAppend(&text, ", classes=");
    textPrintf(&text, "%s", Regex_classes(regex) ? "set" : "null");
    textAppend(&text, " }");
    if (outTruncated)
        *outTruncated = text.truncated;
    return !text.truncated;
}
