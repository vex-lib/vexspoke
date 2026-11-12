// input/focus.c — THE spotlight (one focused window per machine).
//
// One atomic word. Thread 0 writes after reading [NSApp keyWindow] during
// the pump; any thread reads lock-free. No AppKit calls may happen here —
// this module is the firewall between the game thread and the OS.

#include "vex.h"

#include "input/focus.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Focus
 * ============================================================================
 * The single spotlight: one focused window id per machine, stored in one
 * atomic word so any thread can read it lock-free while Thread 0 writes it
 * after reading [NSApp keyWindow] during the event pump. This module is the
 * firewall between the game thread and AppKit — no AppKit calls may happen
 * here. FOCUS_BROADCAST (0) means no window has focus. Lives at R2 as a leaf
 * input behavior consumed by the R1 window pump.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Focus (input/focus.c)
 * ============================================================================
 * THE spotlight (one focused window per machine).
 *
 * STRUCT FIELDS: none — procedural (operates on focused window id (single atomic word))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Focus_id(void)
 *
 * Setters:
 *   - Focus_set(windowId)
 *
 * Getters:
 *   - Focus_isFocused(windowId)
 * ============================================================================
 */


static _Atomic u32 s_focusId = FOCUS_BROADCAST;

void Focus_set(u32 windowId) {
    atomic_store_explicit(&s_focusId, windowId, memory_order_release);
}

u32 Focus_id(void) {
    return atomic_load_explicit(&s_focusId, memory_order_acquire);
}

bool Focus_isFocused(u32 windowId) {
    return Focus_id() == windowId;
}
