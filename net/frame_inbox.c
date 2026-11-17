#include "vex.h"
#include "frame_inbox.h"
// Vexspoke net; caller-fed bounded frame ABI preserved.

#include <time.h>

#include "re/nio/mem.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: FrameInbox
 * ============================================================================
 * A bounded, thread-free frame inbox for the caller (legacy API name:
 * FrameInbox). This is not a WebSocket implementation. Fixed rx
 * buffer, connection state, cancel flag, and timeout — zero steady-state
 * allocation, no threads, no sockets. Bytes arrive via FrameInbox_feed from a
 * caller; no socket adapter is wired to this inbox in the current workspace.
 * FrameInbox_poll drains pending Bytes with a
 * bounded wait: timeoutNs clamped to FRAME_INBOX_POLL_MAX_NS (100ms), ~1ms
 * nanosleep slices, cancel flag re-checked each slice, false (drop-degrade)
 * on timeout per the Bounded Wait Law.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: FrameInbox (net/frame_inbox.c)
 * ============================================================================
 * A bounded, thread-free frame inbox for the caller (legacy API name:
 * FrameInbox). This is not a WebSocket implementation.
 * Fixed rx buffer, connection state, cancel flag, and timeout — zero
 * steady-state allocation, no threads, no sockets. Bytes arrive via
 * FrameInbox_feed from its caller; a socket adapter is not implemented here.
 * FrameInbox_poll
 * drains pending Bytes with a bounded wait: timeoutNs clamped to
 * FRAME_INBOX_POLL_MAX_NS (100ms, Rule 27), ~1ms nanosleep slices,
 * cancel flag re-checked each slice, false (drop-degrade) on timeout.
 *
 * STRUCT FIELDS (Mirroring net/frame_inbox.h — exactly this file's class):
 * ----------------------------------------------------------------------------
 *   FrameInbox {
 *     FrameInboxState state;             // IDLE/CONNECTING/OPEN/CLOSING/CLOSED
 *     u8 rxBuf[FRAME_INBOX_RX_CAP]; // fixed 4096B pending buffer, no alloc
 *     u32 rxLen;                  // pending Bytes (0..RX_CAP)
 *     u64 timeoutNs;              // default poll budget, clamped Rule 27
 *     bool cancelled;                  // cancel flag; poll drops on sight
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - FrameInbox()               : FrameInbox_0()
 *   - FrameInbox(timeoutNs)      : FrameInbox_1(timeoutNs)
 *
 * Core Functions:
 *   - FrameInbox_free(self)
 *   - FrameInbox_feed(self, Bytes, len)
 *   - FrameInbox_poll(self, timeoutNs, dest, destCap, outLen)
 *   - FrameInbox_cancel(self)
 *   - FrameInbox_clear(self)
 *
 * Setters:
 *   - FrameInbox_setState(self, state)
 *   - FrameInbox_setTimeoutNs(self, timeoutNs)
 *   - FrameInbox_setCancelled(self, cancelled)
 *
 * Getters:
 *   - FrameInbox_getState(self)
 *   - FrameInbox_getTimeoutNs(self)
 *   - FrameInbox_isCancelled(self)
 *   - FrameInbox_getPending(self)
 * ============================================================================
 */


;;GETTER
static inline bool FrameInbox_cancelled(const FrameInbox *s) { return (*s).cancelled; }
;;GETTER
static inline bool *FrameInbox_cancelledMut(FrameInbox *s) { return &(*s).cancelled; }
;;SETTER
static inline fn FrameInbox_setCancelled(FrameInbox *s, bool v) { (*s).cancelled = v; }
;;GETTER
static inline u8 *FrameInbox_rxBufMut(FrameInbox *s) { return (*s).rxBuf; }
;;GETTER
static inline const u8 *FrameInbox_rxBuf(const FrameInbox *s) { return (*s).rxBuf; }
;;GETTER
static inline u32 FrameInbox_rxLen(const FrameInbox *s) { return (*s).rxLen; }
;;GETTER
static inline u32 *FrameInbox_rxLenMut(FrameInbox *s) { return &(*s).rxLen; }
;;SETTER
static inline fn FrameInbox_setRxLen(FrameInbox *s, u32 v) { (*s).rxLen = v; }
;;GETTER
static inline FrameInboxState FrameInbox_state(const FrameInbox *s) { return (*s).state; }
;;GETTER
static inline FrameInboxState *FrameInbox_stateMut(FrameInbox *s) { return &(*s).state; }
;;SETTER
static inline fn FrameInbox_setState(FrameInbox *s, FrameInboxState v) { (*s).state = v; }
;;GETTER
static inline u64 FrameInbox_timeoutNs(const FrameInbox *s) { return (*s).timeoutNs; }
;;GETTER
static inline u64 *FrameInbox_timeoutNsMut(FrameInbox *s) { return &(*s).timeoutNs; }
;;SETTER
static inline fn FrameInbox_setTimeoutNs(FrameInbox *s, u64 v) { (*s).timeoutNs = v; }

// net/frame_inbox.c — legacy FrameInbox API for a caller-fed bounded frame inbox; never blocks
// past FRAME_INBOX_POLL_MAX_NS and never touches a socket or a thread.

/** Read the monotonic clock in nanoseconds for the poll timeout budget. */
static u64 frameInboxNowNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64) ts.tv_sec * 1000000000ULL + (u64) ts.tv_nsec;
}

/** Sleep for one cancel-check interval used by bounded polling. */
static fn frameInboxSleepSlice() {
    struct timespec slice;
    slice.tv_sec = 0;
    slice.tv_nsec = 1000000L;
    nanosleep(&slice, nullptr);
}

/** Allocate and initialize a client slot, clamping its default timeout to the supported maximum. */
static FrameInbox *frameInboxCreate(u64 timeoutNs) {
    FrameInbox *self = (FrameInbox*) Memory_alloc(TYPE_FRAME_INBOX_SINGLETON, sizeof(FrameInbox));
    if (!self)
        return nullptr;
    FrameInbox_setState(self, FRAME_INBOX_IDLE);
    FrameInbox_setRxLen(self, 0);
    if (timeoutNs > FRAME_INBOX_POLL_MAX_NS)
        timeoutNs = FRAME_INBOX_POLL_MAX_NS;
    FrameInbox_setTimeoutNs(self, timeoutNs);
    FrameInbox_setCancelled(self, false);
    return self;
}

// CONSTRUCTORS
/** Construct a client with a zero-duration default poll timeout. */
FrameInbox *FrameInbox_0() {
    return frameInboxCreate(ZERO);
}

/** Construct a client with the supplied default timeout, clamped to the maximum. */
FrameInbox *FrameInbox_1(u64 timeoutNs) {
    return frameInboxCreate(timeoutNs);
}

// CORE FUNCTIONS
/** Release the arena-backed client slot; callers must exclude concurrent polling. */
fn FrameInbox_free(FrameInbox *self) {
    if (!self)
        return;
    Memory_free(self);
}

/** Append caller-fed bytes to the fixed receive slot, rejecting empty or overflowing input. */
bool FrameInbox_feed(FrameInbox *self, const u8 *Bytes, u32 len) {
    if (!self || !Bytes)
        return false;
    if (len == 0)
        return false;
    if (len > FRAME_INBOX_RX_CAP - FrameInbox_rxLen(self))
        return false;
    memcpy(FrameInbox_rxBufMut(self) + FrameInbox_rxLen(self), Bytes, len);
    FrameInbox_setRxLen(self, FrameInbox_rxLen(self) + (len));
    return true;
}

/** Wait within the capped budget for buffered bytes, then copy and consume them if the destination fits. */
bool FrameInbox_poll(FrameInbox *self, u64 timeoutNs, u8 *dest,
                   u32 destCap, u32 *outLen) {
    if (outLen)
        (*outLen) = 0;
    if (!self || !dest || destCap == 0 || !outLen)
        return false;
    if (timeoutNs > FRAME_INBOX_POLL_MAX_NS)
        timeoutNs = FRAME_INBOX_POLL_MAX_NS;
    if (FrameInbox_cancelled(self))
        return false;
    if (FrameInbox_rxLen(self) == 0 && timeoutNs == 0)
        return false;
    u64 start = frameInboxNowNs();
    for (;;) {
        if (FrameInbox_cancelled(self))
            return false;
        if (FrameInbox_rxLen(self) > 0)
            break;
        if (frameInboxNowNs() - start >= timeoutNs)
            return false;
        frameInboxSleepSlice();
    }
    u32 n = FrameInbox_rxLen(self);
    if (n > destCap)
        return false;
    memcpy(dest, FrameInbox_rxBufMut(self), n);
    FrameInbox_setRxLen(self, 0);
    (*outLen) = n;
    return true;
}

/** Set the cancellation flag so current or future polls return without waiting further. */
fn FrameInbox_cancel(FrameInbox *self) {
    if (!self)
        return;
    FrameInbox_setCancelled(self, true);
}

/** Discard all pending receive bytes without changing connection or cancellation state. */
fn FrameInbox_clear(FrameInbox *self) {
    if (!self)
        return;
    FrameInbox_setRxLen(self, 0);
}

// SETTERS
/** Replace the stored connection state when the client pointer is non-null. */
fn FrameInbox_setState(FrameInbox *self, FrameInboxState state) {
    if (!self)
        return;
    FrameInbox_setState(self, state);
}

/** Set the default poll timeout, clamped to the maximum permitted interval. */
fn FrameInbox_setTimeoutNs(FrameInbox *self, u64 timeoutNs) {
    if (!self)
        return;
    if (timeoutNs > FRAME_INBOX_POLL_MAX_NS)
        timeoutNs = FRAME_INBOX_POLL_MAX_NS;
    FrameInbox_setTimeoutNs(self, timeoutNs);
}

/** Set or clear the poll cancellation flag. */
fn FrameInbox_setCancelled(FrameInbox *self, bool cancelled) {
    if (!self)
        return;
    FrameInbox_setCancelled(self, cancelled);
}

// GETTERS
/** Return the stored state, using CLOSED as the null-pointer default. */
FrameInboxState FrameInbox_getState(const FrameInbox *self) {
    if (!self)
        return FRAME_INBOX_CLOSED;
    return FrameInbox_state(self);
}

/** Return the stored default poll timeout, or zero for a null pointer. */
u64 FrameInbox_getTimeoutNs(const FrameInbox *self) {
    if (!self)
        return 0;
    return FrameInbox_timeoutNs(self);
}

/** Return the cancellation flag, treating a null client as cancelled. */
bool FrameInbox_isCancelled(const FrameInbox *self) {
    if (!self)
        return true;
    return FrameInbox_cancelled(self);
}

/** Return the number of buffered receive bytes, or zero for a null client. */
u32 FrameInbox_getPending(const FrameInbox *self) {
    if (!self)
        return 0;
    return FrameInbox_rxLen(self);
}
