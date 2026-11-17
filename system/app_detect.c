#include "vex.h"
#include "system/app_detect.h"
#include "system/process_probe.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: AppDetect
 * ============================================================================
 * Local application detection for the engine: "is opencode installed?" via
 * PATH resolution, /Applications bundle scans, and live process liveness via
 * ProcessProbe, over a hand-curated L1 registry of known developer tools
 * (coding CLIs + agent harnesses). No exec, no subprocess, no allocation —
 * pure getenv/access/libproc probes into caller-owned scratch. The singleton
 * AppDetect handle is a zero-init marker; all data lives in the static const
 * AppSlot rows from src/system/data/apps.inc.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: AppDetect (system/app_detect)
 * POSIX/libproc probes; swappable/extendable with zero code changes)
 * ============================================================================
 * Local application detection for the engine: "is opencode installed?"
 * via PATH resolution, /Applications bundle scans, and live process
 * liveness via ProcessProbe, over a hand-curated L1 registry of known
 * developer tools (coding CLIs + agent harnesses). No exec, no
 * subprocess, no allocation — pure getenv/access/libproc probes into
 * caller-owned scratch.
 *
 * STRUCT FIELDS (Mirroring system/app_detect.h — exactly this file's
 * class):
 * ----------------------------------------------------------------------------
 *   u32 reserved;   // singleton marker; no mutable state — all data
 *                        // lives in the static const AppSlot rows
 *
 * SLOT RECORD (AppSlot — Rule 3 co-location, zero behavior of its own):
 * ----------------------------------------------------------------------------
 *   const char *name;         // canonical executable/bundle name
 *   const char *altProcs;     // space-separated alt proc basenames
 *   const char *displayName;  // human label
 *   const char *note;         // what it is / where it lands; NULL when none
 *
 * PRIVATE HELPERS (data only, per-field roles = the SLOT RECORD above):
 * ----------------------------------------------------------------------------
 *   kKnownApps[] — src/system/data/apps.inc (14 hand-curated rows:
 *                  opencode, codex, claude, t3-code, cursor, hermes,
 *                  nous, grok, gemini, aider, goose, cline, qwen-code,
 *                  continue)
 *   kKnownAppCount — row count constant
 *   slotProcessRunning(self, slot) — static — true when name OR any
 *                  altProcs token matches a live process (ProcessProbe)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructor:
 *   - AppDetect_shared() : returns the singleton detection handle
 *
 * Core Functions (POSIX probes, no exec):
 *   - AppDetect_which(self, name, outBuf, cap)  : first PATH match
 *   - AppDetect_isOnPath(self, name)            : PATH presence
 *   - AppDetect_isAppBundle(self, name)         : /Applications bundle
 *   - AppDetect_isInstalled(self, slot)         : PATH or bundle
 *   - AppDetect_isRunning(self, slot)           : live process match
 *   - AppDetect_count / at / get(self, ...)     : registry access
 *
 * Getters (Rule 24; null-safe. Setters omitted — immutable rows, waiver):
 *   - AppDetect_getName / getAltProcs / getDisplayName / getNote(self, slot)
 * ============================================================================
 */

;;GETTER
static inline const char * AppSlot_altProcs(const AppSlot *s) { return (*s).altProcs; }
;;GETTER
static inline const char * *AppSlot_altProcsMut(AppSlot *s) { return &(*s).altProcs; }
;;SETTER
static inline fn AppSlot_setAltProcs(AppSlot *s, const char * v) { (*s).altProcs = v; }
;;GETTER
static inline const char * AppSlot_displayName(const AppSlot *s) { return (*s).displayName; }
;;GETTER
static inline const char * *AppSlot_displayNameMut(AppSlot *s) { return &(*s).displayName; }
;;SETTER
static inline fn AppSlot_setDisplayName(AppSlot *s, const char * v) { (*s).displayName = v; }
;;GETTER
static inline const char * AppSlot_name(const AppSlot *s) { return (*s).name; }
;;GETTER
static inline const char * *AppSlot_nameMut(AppSlot *s) { return &(*s).name; }
;;SETTER
static inline fn AppSlot_setName(AppSlot *s, const char * v) { (*s).name = v; }
;;GETTER
static inline const char * AppSlot_note(const AppSlot *s) { return (*s).note; }
;;GETTER
static inline const char * *AppSlot_noteMut(AppSlot *s) { return &(*s).note; }
;;SETTER
static inline fn AppSlot_setNote(AppSlot *s, const char * v) { (*s).note = v; }

;;INTENTION("probes are PATH-scan + bundle-scan + libproc liveness (vexspoke R1 owns system probes; api-haven consumes for CLI/MCP — PREFERENCES.md Rule 17)")
;;INTENTION("isRunning is a loose basename match by design: enough to answer 'is codex alive'; bundle-id verification is deferred (would need ObjC NSRunningApplication)")
;;INTENTION("immutable registry rows — setters omitted; access via AppDetect_* table functions — Rule 33 Tier-2 waiver")

#include "system/data/apps.inc"

static const u32 kKnownAppCount =
    (u32)(sizeof(kKnownApps) / sizeof(kKnownApps[0]));

static AppDetect sAppDetectShared; // zero-init singleton

// CONSTRUCTORS

AppDetect *AppDetect_shared() {
    return &sAppDetectShared;
}

// CORE FUNCTIONS

bool AppDetect_which(const AppDetect *self, const char *name,
                     char *outBuf, bytes outCap) {
    if (!self || !name || (*name) == '\0' || !outBuf || outCap == 0)
        return false;
    const char *path = getenv("PATH");
    if (!path)
        return false;
    const bytes nameLen = strlen(name);
    const char *dir = path;
    for (;;) {
        const char *sep = strchr(dir, ':');
        const bytes dirLen = sep ? (bytes)(sep - dir) : strlen(dir);
        if (dirLen + 1 + nameLen + 1 <= outCap) {
            memcpy(outBuf, dir, dirLen);
            outBuf[dirLen] = '/';
            memcpy(outBuf + dirLen + 1, name, nameLen);
            outBuf[dirLen + 1 + nameLen] = '\0';
            if (access(outBuf, X_OK) == 0)
                return true;
        }
        if (!sep)
            break;
        dir = sep + 1;
    }
    if (outCap > 0)
        outBuf[0] = '\0';
    return false;
}

bool AppDetect_isOnPath(const AppDetect *self, const char *name) {
    char scratch[4096];
    return AppDetect_which(self, name, scratch, sizeof(scratch));
}

bool AppDetect_isAppBundle(const AppDetect *self, const char *name) {
    if (!self || !name || (*name) == '\0')
        return false;
    // Exact bundle as given, then display-style case of the first letter.
    char bundle[512];
    const bytes n = strlen(name);
    if (n + 6 > sizeof(bundle))
        return false; // "/Applications/" + name + ".app"
    memcpy(bundle, "/Applications/", 14);
    memcpy(bundle + 14, name, n);
    memcpy(bundle + 14 + n, ".app", 5); // includes NUL
    if (access(bundle, F_OK) == 0)
        return true;
    if (n + 6 > sizeof(bundle))
        return false;
    bundle[14] = (char)toupper((u8)bundle[14]);
    return access(bundle, F_OK) == 0;
}

bool AppDetect_isInstalled(const AppDetect *self, const AppSlot *slot) {
    if (!self || !slot)
        return false;
    if (AppDetect_isOnPath(self, AppSlot_name(slot)))
        return true;
    return AppDetect_isAppBundle(self, AppSlot_name(slot));
}

// Static helper: any of the slot's proc names alive right now.
static bool slotProcessRunning(const AppDetect *self, const AppSlot *slot) {
    ProcessProbe *probe = ProcessProbe_shared();
    if (ProcessProbe_isRunning(probe, AppSlot_name(slot)))
        return true;
    const char *alt = AppSlot_altProcs(slot);
    if (!alt || (*alt) == '\0')
        return false;
    const char *p = alt;
    for (;;) {
        while ((*p) == ' ')
            p++;
        if ((*p) == '\0')
            break;
        const char *start = p;
        while ((*p) != '\0' && (*p) != ' ')
            p++;
        const bytes len = (bytes)(p - start);
        char buf[256]; // proc basenames are well under this
        if (len < sizeof(buf)) {
            memcpy(buf, start, len);
            buf[len] = '\0';
            if (ProcessProbe_isRunning(probe, buf))
                return true;
        }
    }
    (discard)self;
    return false;
}

bool AppDetect_isRunning(const AppDetect *self, const AppSlot *slot) {
    if (!self || !slot)
        return false;
    return slotProcessRunning(self, slot);
}

u32 AppDetect_count(const AppDetect *self) {
    if (!self)
        return 0;
    return kKnownAppCount;
}

const AppSlot *AppDetect_at(const AppDetect *self, u32 i) {
    if (!self || i >= kKnownAppCount)
        return nullptr;
    return &kKnownApps[i];
}

const AppSlot *AppDetect_get(const AppDetect *self, const char *name) {
    if (!self || !name || (*name) == '\0')
        return nullptr;
    for (u32 i = 0; i < kKnownAppCount; i++) {
        if (strcmp(kKnownApps[i].name, name) == 0)
            return &kKnownApps[i];
    }
    return nullptr;
}

// GETTERS

const char *AppDetect_getName(const AppDetect *self, const AppSlot *slot) {
    (discard)self;
    return slot ? AppSlot_name(slot) : nullptr;
}

const char *AppDetect_getAltProcs(const AppDetect *self, const AppSlot *slot) {
    (discard)self;
    return slot ? AppSlot_altProcs(slot) : nullptr;
}

const char *AppDetect_getDisplayName(const AppDetect *self, const AppSlot *slot) {
    (discard)self;
    return slot ? AppSlot_displayName(slot) : nullptr;
}

const char *AppDetect_getNote(const AppDetect *self, const AppSlot *slot) {
    (discard)self;
    return slot ? AppSlot_note(slot) : nullptr;
}
