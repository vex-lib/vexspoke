#include "vex.h"
#include "system/display_monitor.h"

#include <stdlib.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Display_monitor
 * ============================================================================
 * Single display monitor representation: id, name, current/native pixel
 * resolutions, logical point size, refresh rate, HDR flag, and DPI scale —
 * the fields the system probe fills and the window/compositor layers read.
 * The struct is arena-allocated zeroed with dpi defaulting to 1.0, and every
 * field has a symmetric getter/setter pair per the Symmetric Getter/Setter
 * Completeness Law; getters return safe defaults on null.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Display_monitor (system/display_monitor.c)
 * ============================================================================
 * single display monitor representation (Legacy: system/DisplayMonitor.java)
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   DisplayMonitor {
 *     u32 id;               // monitor identifier
 *     char name[128];            // display name string
 *     i32 currentWidth;      // active physical pixel width
 *     i32 currentHeight;     // active physical pixel height
 *     i32 pointWidth;        // logical point width (UI space)
 *     i32 pointHeight;       // logical point height (UI space)
 *     i32 nativeWidth;       // hardware panel native width
 *     i32 nativeHeight;      // hardware panel native height
 *     i32 refreshRate;       // refresh rate in Hz
 *     bool hdrSupported;         // HDR capability flag
 *     f32 dpi;                 // density / scale factor
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - DisplayMonitor_0(void)
 *
 * Core Functions:
 *   - DisplayMonitor_free(monitor)
 *
 * Setters:
 *   - DisplayMonitor_setId(m, id)
 *   - DisplayMonitor_setName(m, name)
 *   - DisplayMonitor_setCurrentWidth(m, val)
 *   - DisplayMonitor_setCurrentHeight(m, val)
 *   - DisplayMonitor_setPointWidth(m, val)
 *   - DisplayMonitor_setPointHeight(m, val)
 *   - DisplayMonitor_setNativeWidth(m, val)
 *   - DisplayMonitor_setNativeHeight(m, val)
 *   - DisplayMonitor_setRefreshRate(m, val)
 *   - DisplayMonitor_setHdrSupported(m, val)
 *   - DisplayMonitor_setDpi(m, val)
 *
 * Getters:
 *   - DisplayMonitor_getId(m)
 *   - DisplayMonitor_getName(m)
 *   - DisplayMonitor_getCurrentWidth(m)
 *   - DisplayMonitor_getCurrentHeight(m)
 *   - DisplayMonitor_getPointWidth(m)
 *   - DisplayMonitor_getPointHeight(m)
 *   - DisplayMonitor_getNativeWidth(m)
 *   - DisplayMonitor_getNativeHeight(m)
 *   - DisplayMonitor_getRefreshRate(m)
 *   - DisplayMonitor_getHdrSupported(m)
 *   - DisplayMonitor_getDpi(m)
 * ============================================================================
 */


struct DisplayMonitor {
    u32 id;
    char name[128];
    i32 currentWidth;   // active physical pixels
    i32 currentHeight;
    i32 pointWidth;     // logical points
    i32 pointHeight;
    i32 nativeWidth;    // hardware panel native pixels
    i32 nativeHeight;
    i32 refreshRate;    // Hz
    bool hdrSupported;
    f32 dpi;              // scale factor / density
};

DisplayMonitor *DisplayMonitor_0(void) {
    DisplayMonitor *m = (DisplayMonitor*) Memory_alloc(TYPE_DISPLAY_MONITOR_SINGLETON, sizeof(DisplayMonitor));
    if (!m)
        return nullptr;
    memset(m, 0, sizeof(DisplayMonitor));
    (*m).dpi = 1.0f;
    return m;
}

void DisplayMonitor_free(DisplayMonitor *monitor) {
    if (!monitor)
        return;
    Memory_free(monitor);
}

u32 DisplayMonitor_getId(const DisplayMonitor *m) {
    return m ? (*m).id : 0;
}

void DisplayMonitor_setId(DisplayMonitor *m, u32 id) {
    if (m) (*m).id = id;
}

const char *DisplayMonitor_getName(const DisplayMonitor *m) {
    return m ? (*m).name : "";
}

void DisplayMonitor_setName(DisplayMonitor *m, const char *name) {
    if (!m) return;
    if (name) {
        strncpy((*m).name, name, sizeof((*m).name) - 1);
        (*m).name[sizeof((*m).name) - 1] = '\0';
    } else {
        (*m).name[0] = '\0';
    }
}

i32 DisplayMonitor_getCurrentWidth(const DisplayMonitor *m) {
    return m ? (*m).currentWidth : 0;
}

void DisplayMonitor_setCurrentWidth(DisplayMonitor *m, i32 val) {
    if (m) (*m).currentWidth = val;
}

i32 DisplayMonitor_getCurrentHeight(const DisplayMonitor *m) {
    return m ? (*m).currentHeight : 0;
}

void DisplayMonitor_setCurrentHeight(DisplayMonitor *m, i32 val) {
    if (m) (*m).currentHeight = val;
}

i32 DisplayMonitor_getPointWidth(const DisplayMonitor *m) {
    return m ? (*m).pointWidth : 0;
}

void DisplayMonitor_setPointWidth(DisplayMonitor *m, i32 val) {
    if (m) (*m).pointWidth = val;
}

i32 DisplayMonitor_getPointHeight(const DisplayMonitor *m) {
    return m ? (*m).pointHeight : 0;
}

void DisplayMonitor_setPointHeight(DisplayMonitor *m, i32 val) {
    if (m) (*m).pointHeight = val;
}

i32 DisplayMonitor_getNativeWidth(const DisplayMonitor *m) {
    return m ? (*m).nativeWidth : 0;
}

void DisplayMonitor_setNativeWidth(DisplayMonitor *m, i32 val) {
    if (m) (*m).nativeWidth = val;
}

i32 DisplayMonitor_getNativeHeight(const DisplayMonitor *m) {
    return m ? (*m).nativeHeight : 0;
}

void DisplayMonitor_setNativeHeight(DisplayMonitor *m, i32 val) {
    if (m) (*m).nativeHeight = val;
}

i32 DisplayMonitor_getRefreshRate(const DisplayMonitor *m) {
    return m ? (*m).refreshRate : 0;
}

void DisplayMonitor_setRefreshRate(DisplayMonitor *m, i32 val) {
    if (m) (*m).refreshRate = val;
}

bool DisplayMonitor_getHdrSupported(const DisplayMonitor *m) {
    return m ? (*m).hdrSupported : false;
}

void DisplayMonitor_setHdrSupported(DisplayMonitor *m, bool val) {
    if (m) (*m).hdrSupported = val;
}

f32 DisplayMonitor_getDpi(const DisplayMonitor *m) {
    return m ? (*m).dpi : 1.0f;
}

void DisplayMonitor_setDpi(DisplayMonitor *m, f32 val) {
    if (m) (*m).dpi = val;
}
