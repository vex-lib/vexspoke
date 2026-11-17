#include "vex.h"
#include <stdarg.h>

#include "lang/str.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Str
 * ============================================================================
 * The bounded string builder — the engine the toString Law stands on. Every
 * write is capped at `cap` and flags `truncated` on a cut, so a class never
 * hand-rolls a bounds check and a deep/large dump can never overflow. The buffer
 * is always null-terminated (one byte reserved).
 *
 * Str owns nothing: it borrows the caller's buffer. It is the Cold-Strict,
 * Hot-Minimal Validation Law's Truncation-Never-Silent clause made concrete —
 * a cut is a loud flag, never a silent overwrite.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Str (lang/str.c)
 * ============================================================================
 * SUMMARY:
 *   Borrows a destination buffer; every append is bounded and flags truncation.
 *
 * STRUCT FIELDS (Mirroring lang/str.h):
 * ----------------------------------------------------------------------------
 *   char *dest;       // borrowed destination buffer
 *   bytes cap;       // total capacity (including the terminator)
 *   bytes len;       // Bytes written so far (excluding the terminator)
 *   bool truncated;   // a write was cut
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   (none)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Constructors: (.h)
 *   - Str(dest, cap)
 *
 * Public Core Functions: (.h)
 *   - Str_put(s, text) / Str_putc(s, c) / Str_putQuoted(s, text) / Str_printf(s, fmt, ...)
 *
 * Public Getters: (.h)
 *   - Str_isTruncated(s) / Str_length(s) / Str_cstr(s)
 * ============================================================================
 */


;;GETTER
static inline bytes Str_cap(const Str *s) { return (*s).cap; }
;;GETTER
static inline bytes *Str_capMut(Str *s) { return &(*s).cap; }
;;SETTER
static inline fn Str_setCap(Str *s, bytes v) { (*s).cap = v; }
;;GETTER
static inline char * Str_dest(const Str *s) { return (*s).dest; }
;;GETTER
static inline char * *Str_destMut(Str *s) { return &(*s).dest; }
;;SETTER
static inline fn Str_setDest(Str *s, char * v) { (*s).dest = v; }
;;GETTER
static inline bytes Str_len(const Str *s) { return (*s).len; }
;;GETTER
static inline bytes *Str_lenMut(Str *s) { return &(*s).len; }
;;SETTER
static inline fn Str_setLen(Str *s, bytes v) { (*s).len = v; }
;;GETTER
static inline bool Str_truncated(const Str *s) { return (*s).truncated; }
;;GETTER
static inline bool *Str_truncatedMut(Str *s) { return &(*s).truncated; }
;;SETTER
static inline fn Str_setTruncated(Str *s, bool v) { (*s).truncated = v; }

// Remaining writable Bytes (excluding the reserved terminator).
static bytes room(const Str *s) {
    if (s == nullptr || Str_dest(s) == nullptr || Str_cap(s) == 0)
        return 0;
    return (Str_len(s) < Str_cap(s) - 1u) ? (Str_cap(s) - 1u - Str_len(s)) : 0u;
}

Str Str_2(char *dest, bytes cap) {
    Str result;
    result.dest = dest;
    result.cap = cap;
    result.len = 0u;
    result.truncated = false;
    if (dest != nullptr && cap > 0u)
        dest[0] = '\0';
    return result;
}

fn Str_putc(Str *s, char c) {
    if (s == nullptr)
        return;
    if (Str_dest(s) == nullptr || Str_cap(s) == 0) {
        Str_setTruncated(s, true);
        return;
    }
    if (Str_len(s) + 1u >= Str_cap(s)) {
        Str_setTruncated(s, true);
        return;
    }
    Str_dest(s)[(*Str_lenMut(s))++] = c;
    Str_dest(s)[Str_len(s)] = '\0';
}

fn Str_put(Str *s, const char *text) {
    if (s == nullptr || text == nullptr)
        return;
    bytes n = strlen(text);
    bytes r = room(s);
    if (n > r) {
        n = r;
        Str_setTruncated(s, true);
    }
    if (n > 0) {
        memcpy(Str_dest(s) + Str_len(s), text, n);
        Str_setLen(s, Str_len(s) + (n));
        Str_dest(s)[Str_len(s)] = '\0';
    }
}

fn Str_putQuoted(Str *s, const char *text) {
    if (s == nullptr)
        return;
    Str_putc(s, '"');
    if (text != nullptr) {
        for (const char *p = text; *p != '\0'; p++) {
            switch (*p) {
                case '\n': Str_put(s, "\\n"); break;
                case '\t': Str_put(s, "\\t"); break;
                case '\r': Str_put(s, "\\r"); break;
                case '"':  Str_put(s, "\\\""); break;
                case '\\': Str_put(s, "\\\\"); break;
                default:   Str_putc(s, *p); break;
            }
        }
    }
    Str_putc(s, '"');
}

fn Str_printf(Str *s, const char *fmt, ...) {
    if (s == nullptr || fmt == nullptr)
        return;
    if (Str_dest(s) == nullptr || Str_cap(s) == 0) {
        Str_setTruncated(s, true);
        return;
    }
    va_list args;
    va_start(args, fmt);
    i32 written = vsnprintf(Str_dest(s) + Str_len(s), Str_cap(s) - Str_len(s), fmt, args);
    va_end(args);
    if (written < 0) {
        Str_setTruncated(s, true);
        return;
    }
    bytes w = (bytes) written;
    bytes r = room(s);
    if (w > r) {
        Str_setLen(s, Str_cap(s) - 1u);   // vsnprintf wrote r Bytes + terminator
        Str_setTruncated(s, true);
    } else {
        Str_setLen(s, Str_len(s) + (w));
    }
}

;;GETTER
bool Str_isTruncated(const Str *s) {
    return s ? Str_truncated(s) : false;
}

;;GETTER
bytes Str_length(const Str *s) {
    return s ? Str_len(s) : 0u;
}

;;GETTER
const char *Str_cstr(const Str *s) {
    return (s && Str_dest(s)) ? Str_dest(s) : "";
}
