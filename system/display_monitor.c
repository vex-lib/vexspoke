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
 *   - DisplayMonitor_0()
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

;;GETTER
static inline i32 DisplayMonitor_currentHeight(const DisplayMonitor *s) { return (*s).currentHeight; }
;;GETTER
static inline i32 *DisplayMonitor_currentHeightMut(DisplayMonitor *s) { return &(*s).currentHeight; }
;;GETTER
static inline i32 DisplayMonitor_currentWidth(const DisplayMonitor *s) { return (*s).currentWidth; }
;;GETTER
static inline i32 *DisplayMonitor_currentWidthMut(DisplayMonitor *s) { return &(*s).currentWidth; }
;;GETTER
static inline f32 DisplayMonitor_dpi(const DisplayMonitor *s) { return (*s).dpi; }
;;GETTER
static inline f32 *DisplayMonitor_dpiMut(DisplayMonitor *s) { return &(*s).dpi; }
;;GETTER
static inline bool DisplayMonitor_hdrSupported(const DisplayMonitor *s) { return (*s).hdrSupported; }
;;GETTER
static inline bool *DisplayMonitor_hdrSupportedMut(DisplayMonitor *s) { return &(*s).hdrSupported; }
;;GETTER
static inline u32 DisplayMonitor_id(const DisplayMonitor *s) { return (*s).id; }
;;GETTER
static inline u32 *DisplayMonitor_idMut(DisplayMonitor *s) { return &(*s).id; }
;;GETTER
static inline char *DisplayMonitor_nameMut(DisplayMonitor *s) { return (*s).name; }
;;GETTER
static inline const char *DisplayMonitor_name(const DisplayMonitor *s) { return (*s).name; }
;;GETTER
static inline i32 DisplayMonitor_nativeHeight(const DisplayMonitor *s) { return (*s).nativeHeight; }
;;GETTER
static inline i32 *DisplayMonitor_nativeHeightMut(DisplayMonitor *s) { return &(*s).nativeHeight; }
;;GETTER
static inline i32 DisplayMonitor_nativeWidth(const DisplayMonitor *s) { return (*s).nativeWidth; }
;;GETTER
static inline i32 *DisplayMonitor_nativeWidthMut(DisplayMonitor *s) { return &(*s).nativeWidth; }
;;GETTER
static inline i32 DisplayMonitor_pointHeight(const DisplayMonitor *s) { return (*s).pointHeight; }
;;GETTER
static inline i32 *DisplayMonitor_pointHeightMut(DisplayMonitor *s) { return &(*s).pointHeight; }
;;GETTER
static inline i32 DisplayMonitor_pointWidth(const DisplayMonitor *s) { return (*s).pointWidth; }
;;GETTER
static inline i32 *DisplayMonitor_pointWidthMut(DisplayMonitor *s) { return &(*s).pointWidth; }
;;GETTER
static inline i32 DisplayMonitor_refreshRate(const DisplayMonitor *s) { return (*s).refreshRate; }
;;GETTER
static inline i32 *DisplayMonitor_refreshRateMut(DisplayMonitor *s) { return &(*s).refreshRate; }


DisplayMonitor *DisplayMonitor_0() {
    DisplayMonitor *m = (DisplayMonitor*) Memory_alloc(TYPE_DISPLAY_MONITOR_SINGLETON, sizeof(DisplayMonitor));
    if (!m)
        return nullptr;
    memset(m, 0, sizeof(DisplayMonitor));
    DisplayMonitor_setDpi(m, 1.0f);
    return m;
}

fn DisplayMonitor_free(DisplayMonitor *monitor) {
    if (!monitor)
        return;
    Memory_free(monitor);
}

u32 DisplayMonitor_getId(const DisplayMonitor *m) {
    return m ? DisplayMonitor_id(m) : 0;
}

fn DisplayMonitor_setId(DisplayMonitor *m, u32 id) {
    if (m) (*DisplayMonitor_idMut(m)) = id;
}

const char *DisplayMonitor_getName(const DisplayMonitor *m) {
    return m ? DisplayMonitor_name(m) : "";
}

fn DisplayMonitor_setName(DisplayMonitor *m, const char *name) {
    if (!m) return;
    if (name) {
        strncpy(DisplayMonitor_nameMut(m), name, sizeof ((DisplayMonitor){0}).name - 1);
        DisplayMonitor_nameMut(m)[sizeof ((DisplayMonitor){0}).name - 1] = '\0';
    } else {
        DisplayMonitor_nameMut(m)[0] = '\0';
    }
}

i32 DisplayMonitor_getCurrentWidth(const DisplayMonitor *m) {
    return m ? DisplayMonitor_currentWidth(m) : 0;
}

fn DisplayMonitor_setCurrentWidth(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_currentWidthMut(m)) = val;
}

i32 DisplayMonitor_getCurrentHeight(const DisplayMonitor *m) {
    return m ? DisplayMonitor_currentHeight(m) : 0;
}

fn DisplayMonitor_setCurrentHeight(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_currentHeightMut(m)) = val;
}

i32 DisplayMonitor_getPointWidth(const DisplayMonitor *m) {
    return m ? DisplayMonitor_pointWidth(m) : 0;
}

fn DisplayMonitor_setPointWidth(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_pointWidthMut(m)) = val;
}

i32 DisplayMonitor_getPointHeight(const DisplayMonitor *m) {
    return m ? DisplayMonitor_pointHeight(m) : 0;
}

fn DisplayMonitor_setPointHeight(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_pointHeightMut(m)) = val;
}

i32 DisplayMonitor_getNativeWidth(const DisplayMonitor *m) {
    return m ? DisplayMonitor_nativeWidth(m) : 0;
}

fn DisplayMonitor_setNativeWidth(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_nativeWidthMut(m)) = val;
}

i32 DisplayMonitor_getNativeHeight(const DisplayMonitor *m) {
    return m ? DisplayMonitor_nativeHeight(m) : 0;
}

fn DisplayMonitor_setNativeHeight(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_nativeHeightMut(m)) = val;
}

i32 DisplayMonitor_getRefreshRate(const DisplayMonitor *m) {
    return m ? DisplayMonitor_refreshRate(m) : 0;
}

fn DisplayMonitor_setRefreshRate(DisplayMonitor *m, i32 val) {
    if (m) (*DisplayMonitor_refreshRateMut(m)) = val;
}

bool DisplayMonitor_getHdrSupported(const DisplayMonitor *m) {
    return m ? DisplayMonitor_hdrSupported(m) : false;
}

fn DisplayMonitor_setHdrSupported(DisplayMonitor *m, bool val) {
    if (m) (*DisplayMonitor_hdrSupportedMut(m)) = val;
}

f32 DisplayMonitor_getDpi(const DisplayMonitor *m) {
    return m ? DisplayMonitor_dpi(m) : 1.0f;
}

fn DisplayMonitor_setDpi(DisplayMonitor *m, f32 val) {
    if (m) (*DisplayMonitor_dpiMut(m)) = val;
}
