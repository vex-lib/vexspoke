#include "vex.h"
#include "system/display_info.h"

#include "system/discovery.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Display_info
 * ============================================================================
 * Global display subsystem query facade (Legacy: system/DisplayInfo.java).
 * Lazily bootstraps the monitor registry through SystemDiscovery on first
 * getter call, then serves cached monitor list, primary monitor, resolution
 * (monitor/point/native), refresh rate, HDR, and density state through
 * symmetric getters/setters. The registry is a fixed 16-slot array of
 * DisplayMonitor pointers — a bounded hardware reality, not a workload
 * ceiling. Setters are cold-path test seams; getters return safe defaults.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Display_info (system/display_info.c)
 * ============================================================================
 * global display subsystem query (Legacy: system/DisplayInfo.java)
 *
 * STRUCT FIELDS: none — procedural (operates on DisplayMonitor registry (primary + list))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Setters:
 *   - DisplayInfo_setMonitorCount(val)
 *   - DisplayInfo_setPrimaryMonitor(val)
 *   - DisplayInfo_setMonitors(monitors, count)
 *   - DisplayInfo_setMonitorResolutionWidth(val)
 *   - DisplayInfo_setMonitorResolutionHeight(val)
 *   - DisplayInfo_setPointResolutionWidth(val)
 *   - DisplayInfo_setPointResolutionHeight(val)
 *   - DisplayInfo_setNativeResolutionWidth(val)
 *   - DisplayInfo_setNativeResolutionHeight(val)
 *   - DisplayInfo_setCurrentRefreshRate(val)
 *   - DisplayInfo_setHdrSupported(val)
 *   - DisplayInfo_setDisplayDensity(val)
 *   - DisplayInfo_setHardwareDensity(val)
 *
 * Getters:
 *   - DisplayInfo_getMonitorCount()
 *   - DisplayInfo_getPrimaryMonitor()
 *   - DisplayInfo_getMonitor(index)
 *   - DisplayInfo_getMonitors(outCount)
 *   - DisplayInfo_getMonitorResolutionWidth()
 *   - DisplayInfo_getMonitorResolutionHeight()
 *   - DisplayInfo_getPointResolutionWidth()
 *   - DisplayInfo_getPointResolutionHeight()
 *   - DisplayInfo_getNativeResolutionWidth()
 *   - DisplayInfo_getNativeResolutionHeight()
 *   - DisplayInfo_getCurrentRefreshRate()
 *   - DisplayInfo_getHdrSupported()
 *   - DisplayInfo_getDisplayDensity()
 *   - DisplayInfo_getHardwareDensity()
 * ============================================================================
 */


#define MAX_SYSTEM_MONITORS 16

static i32         s_monitorCount = 0;
static DisplayMonitor *s_monitors[MAX_SYSTEM_MONITORS] = {0};
static DisplayMonitor *s_primaryMonitor = nullptr;

static i32 s_monitorResolutionWidth = 0;
static i32 s_monitorResolutionHeight = 0;
static i32 s_pointResolutionWidth = 0;
static i32 s_pointResolutionHeight = 0;
static i32 s_nativeResolutionWidth = 0;
static i32 s_nativeResolutionHeight = 0;
static i32 s_currentRefreshRate = 0;
static bool    s_hdrSupported = false;
static f32   s_displayDensity = 1.0f;

static inline fn ensureDiscovered() {
    SystemDiscovery_bootstrap();
}

i32 DisplayInfo_getMonitorCount() {
    ensureDiscovered();
    return s_monitorCount;
}

fn DisplayInfo_setMonitorCount(i32 val) {
    s_monitorCount = val;
}

DisplayMonitor *DisplayInfo_getPrimaryMonitor() {
    ensureDiscovered();
    return s_primaryMonitor;
}

fn DisplayInfo_setPrimaryMonitor(DisplayMonitor *val) {
    s_primaryMonitor = val;
}

DisplayMonitor *DisplayInfo_getMonitor(bytes index) {
    ensureDiscovered();
    if (index < (bytes)s_monitorCount && index < MAX_SYSTEM_MONITORS)
        return s_monitors[index];
    return nullptr;
}

DisplayMonitor **DisplayInfo_getMonitors(bytes *outCount) {
    ensureDiscovered();
    if (outCount)
        *outCount = (bytes)s_monitorCount;
    return s_monitors;
}

fn DisplayInfo_setMonitors(DisplayMonitor **monitors, bytes count) {
    if (count > MAX_SYSTEM_MONITORS)
        count = MAX_SYSTEM_MONITORS;
    for (bytes i = 0; i < count; i++) {
        s_monitors[i] = monitors[i];
    }
    s_monitorCount = (i32)count;
}

i32 DisplayInfo_getMonitorResolutionWidth() {
    ensureDiscovered();
    return s_monitorResolutionWidth;
}

fn DisplayInfo_setMonitorResolutionWidth(i32 val) {
    s_monitorResolutionWidth = val;
}

i32 DisplayInfo_getMonitorResolutionHeight() {
    ensureDiscovered();
    return s_monitorResolutionHeight;
}

fn DisplayInfo_setMonitorResolutionHeight(i32 val) {
    s_monitorResolutionHeight = val;
}

i32 DisplayInfo_getPointResolutionWidth() {
    ensureDiscovered();
    return s_pointResolutionWidth;
}

fn DisplayInfo_setPointResolutionWidth(i32 val) {
    s_pointResolutionWidth = val;
}

i32 DisplayInfo_getPointResolutionHeight() {
    ensureDiscovered();
    return s_pointResolutionHeight;
}

fn DisplayInfo_setPointResolutionHeight(i32 val) {
    s_pointResolutionHeight = val;
}

i32 DisplayInfo_getNativeResolutionWidth() {
    ensureDiscovered();
    return s_nativeResolutionWidth;
}

fn DisplayInfo_setNativeResolutionWidth(i32 val) {
    s_nativeResolutionWidth = val;
}

i32 DisplayInfo_getNativeResolutionHeight() {
    ensureDiscovered();
    return s_nativeResolutionHeight;
}

fn DisplayInfo_setNativeResolutionHeight(i32 val) {
    s_nativeResolutionHeight = val;
}

i32 DisplayInfo_getCurrentRefreshRate() {
    ensureDiscovered();
    return s_currentRefreshRate;
}

fn DisplayInfo_setCurrentRefreshRate(i32 val) {
    s_currentRefreshRate = val;
}

bool DisplayInfo_getHdrSupported() {
    ensureDiscovered();
    return s_hdrSupported;
}

fn DisplayInfo_setHdrSupported(bool val) {
    s_hdrSupported = val;
}

f32 DisplayInfo_getDisplayDensity() {
    ensureDiscovered();
    return s_displayDensity;
}

fn DisplayInfo_setDisplayDensity(f32 val) {
    s_displayDensity = val;
}

static f32 s_hardwareDensity = 1.0f;

f32 DisplayInfo_getHardwareDensity() {
    ensureDiscovered();
    return s_hardwareDensity;
}

fn DisplayInfo_setHardwareDensity(f32 val) {
    s_hardwareDensity = val;
}
