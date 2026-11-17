#include "vex.h"
#include "exception.h"

#include <stdlib.h>

;;DEFINITION
/** Exception owns formatted diagnostic strings and source metadata for a cold
 * failure report. Formatting and printing may allocate or block, so this is a
 * diagnostic/lifecycle object rather than a real-time error channel. */
;;OVERVIEW
/** ========================================================================
 * CLASS: Exception (exception/exception.c)
 * STRUCT FIELDS (Mirroring exception.h)
 * u64 type                 project-scoped Exception singleton identity
 * ExceptionCategory category    stable diagnostic category
 * const char *site              borrowed operation/site label
 * const char *file              borrowed source filename
 * i32 line                      source line number
 * char *message                 owned formatted primary diagnostic
 * char *details                 owned formatted context diagnostic
 * FUNCTION REGISTRY
 * Projections: const char *Exception_categoryName(ExceptionCategory category)
 * Constructors: Exception_0(), Exception_5(..), Exception_6(..),
 *   Exception_7(..), Exception_8(..)
 * Lifecycle: fn Exception_free(Exception *self)
 * Setters: fn Exception_setDetails(Exception *self, const char *fmt, ...)
 *   fn Exception_setDetailsV(Exception *self, const char *fmt, va_list args)
 * Operations: fn Exception_print(const Exception *self)
 * Private behavior: format sizing/copying is performed by exceptionInitializeV
 *   and setDetailsV;
 *   allocation failure preserves the prior details value on replacement.
 * Null/default, failure and ownership contract:
 * Exception_0 produces a valid empty diagnostic. Null self is ignored by
 * setters, free and print. Formatted message/details allocations are owned by
 * Exception and released by free;
 * site/file strings are borrowed. stderr formatting is cold and synchronous.
 * ======================================================================== */


;;GETTER
static inline ExceptionCategory Exception_category(const Exception *s) { return (*s).category; }
;;GETTER
static inline ExceptionCategory *Exception_categoryMut(Exception *s) { return &(*s).category; }
;;SETTER
static inline fn Exception_setCategory(Exception *s, ExceptionCategory v) { (*s).category = v; }
;;GETTER
static inline char * Exception_details(const Exception *s) { return (*s).details; }
;;GETTER
static inline char * *Exception_detailsMut(Exception *s) { return &(*s).details; }
;;SETTER
static inline fn Exception_setDetails(Exception *s, char * v) { (*s).details = v; }
;;GETTER
static inline const char * Exception_file(const Exception *s) { return (*s).file; }
;;GETTER
static inline const char * *Exception_fileMut(Exception *s) { return &(*s).file; }
;;SETTER
static inline fn Exception_setFile(Exception *s, const char * v) { (*s).file = v; }
;;GETTER
static inline i32 Exception_line(const Exception *s) { return (*s).line; }
;;GETTER
static inline i32 *Exception_lineMut(Exception *s) { return &(*s).line; }
;;SETTER
static inline fn Exception_setLine(Exception *s, i32 v) { (*s).line = v; }
;;GETTER
static inline char * Exception_message(const Exception *s) { return (*s).message; }
;;GETTER
static inline char * *Exception_messageMut(Exception *s) { return &(*s).message; }
;;SETTER
static inline fn Exception_setMessage(Exception *s, char * v) { (*s).message = v; }
;;GETTER
static inline const char * Exception_site(const Exception *s) { return (*s).site; }
;;GETTER
static inline const char * *Exception_siteMut(Exception *s) { return &(*s).site; }
;;SETTER
static inline fn Exception_setSite(Exception *s, const char * v) { (*s).site = v; }
;;GETTER
static inline u64 Exception_type(const Exception *s) { return (*s).type; }
;;GETTER
static inline u64 *Exception_typeMut(Exception *s) { return &(*s).type; }
;;SETTER
static inline fn Exception_setType(Exception *s, u64 v) { (*s).type = v; }

// Returns the stable display name for a category, or the unknown-category name.
const char *Exception_categoryName(ExceptionCategory category) {
    switch (category) {
        case EXCEPTION_RUNTIME:        return "RuntimeException";
        case EXCEPTION_WINDOW:         return "WindowException";
        case EXCEPTION_GRAPHICS:       return "GraphicsException";
        case EXCEPTION_NULL_POINTER:   return "NullPointerException";
        case EXCEPTION_ILLEGAL_STATE:  return "IllegalStateException";
        case EXCEPTION_IO:             return "IOException";
        case EXCEPTION_OUT_OF_MEMORY:  return "OutOfMemoryException";
        default:                       return "UnknownException";
    }
}

// Initializes exception metadata and formats an optional message using args.
static fn exceptionInitializeV(Exception *self,
                                 ExceptionCategory category,
                                 const char *site,
                                 const char *file,
                                 i32 line,
                                 const char *fmt,
                                 va_list args) {
    Exception_setType(self, TYPE_EXCEPTION_SINGLETON);
    Exception_setCategory(self, category);
    Exception_setSite(self, (site != nullptr) ? site : "UnknownSite");
    Exception_setFile(self, (file != nullptr) ? file : "UnknownFile");
    Exception_setLine(self, line);
    Exception_setMessage(self, nullptr);
    Exception_setDetails(self, nullptr);

    if (fmt != nullptr) {
        va_list copy;
        va_copy(copy, args);
        i32 needed = vsnprintf(nullptr, 0, fmt, copy);
        va_end(copy);

        if (needed >= 0) {
            char *buf = (char*) malloc((bytes)needed + 1);
            if (buf != nullptr) {
                vsnprintf(buf, (bytes)needed + 1, fmt, args);
                Exception_setMessage(self, buf);
            }
        }
    }
}

Exception Exception_0() {
    Exception self;
    memset(&self, 0, sizeof self);
    self.type = TYPE_EXCEPTION_SINGLETON;
    self.category = EXCEPTION_RUNTIME;
    self.site = "UnknownSite";
    self.file = "UnknownFile";
    return self;
}

static Exception exceptionFormatted(ExceptionCategory category,
                                     const char *site,
                                     const char *file,
                                     i32 line,
                                     const char *fmt,
                                     va_list args) {
    Exception self = Exception_0();
    exceptionInitializeV(&self, category, site, file, line, fmt, args);
    return self;
}

Exception Exception_5(ExceptionCategory category, const char *site,
                      const char *file, i32 line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Exception self = exceptionFormatted(category, site, file, line, fmt, args);
    va_end(args);
    return self;
}

Exception Exception_6(ExceptionCategory category, const char *site,
                      const char *file, i32 line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Exception self = exceptionFormatted(category, site, file, line, fmt, args);
    va_end(args);
    return self;
}

Exception Exception_7(ExceptionCategory category, const char *site,
                      const char *file, i32 line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Exception self = exceptionFormatted(category, site, file, line, fmt, args);
    va_end(args);
    return self;
}

Exception Exception_8(ExceptionCategory category, const char *site,
                      const char *file, i32 line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Exception self = exceptionFormatted(category, site, file, line, fmt, args);
    va_end(args);
    return self;
}

// Replaces owned details with formatted args when formatting and allocation succeed.
fn Exception_setDetailsV(Exception *self, const char *fmt, va_list args) {
    if (self == nullptr || fmt == nullptr) return;

    va_list copy;
    va_copy(copy, args);
    i32 needed = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);

    if (needed < 0) return;

    char *buf = (char*) malloc((bytes)needed + 1);
    if (buf != nullptr) {
        vsnprintf(buf, (bytes)needed + 1, fmt, args);
        if (Exception_details(self) != nullptr) {
            free(Exception_details(self));
        }
        Exception_setDetails(self, buf);
    }
}

// Replaces owned details with formatted variadic text.
fn Exception_setDetails(Exception *self, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Exception_setDetailsV(self, fmt, args);
    va_end(args);
}

// Releases the owned message and details strings and clears their pointers.
fn Exception_free(Exception *self) {
    if (self == nullptr) return;

    if (Exception_message(self) != nullptr) {
        free(Exception_message(self));
        Exception_setMessage(self, nullptr);
    }
    if (Exception_details(self) != nullptr) {
        free(Exception_details(self));
        Exception_setDetails(self, nullptr);
    }
}

// Writes category, source location, message, and details to stderr.
fn Exception_print(const Exception *self) {
    if (self == nullptr) return;

    fprintf(stderr, "\n================================================================================\n");
    fprintf(stderr, "RUNTIME EXCEPTION: [%s] at %s\n",
            Exception_categoryName(Exception_category(self)),
            Exception_site(self) ? Exception_site(self) : "unknown");
    fprintf(stderr, "Location: %s:%d\n",
            Exception_file(self) ? Exception_file(self) : "unknown",
            Exception_line(self));
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
    if (Exception_message(self) != nullptr && Exception_message(self)[0] != '\0') {
        fprintf(stderr, "Reason / Probable Causes:\n  %s\n", Exception_message(self));
    }
    if (Exception_details(self) != nullptr && Exception_details(self)[0] != '\0') {
        fprintf(stderr, "Variable Dump / Context Diagnostics:\n  %s\n", Exception_details(self));
    }
    fprintf(stderr, "================================================================================\n\n");
    fflush(stderr);
}
