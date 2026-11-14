#ifndef SYSTEM_DISPLAY_INFO_H
#define SYSTEM_DISPLAY_INFO_H

#include "vex.h"
#include "system/display_monitor.h"

// system/display_info.h — global display subsystem query (Legacy: system/DisplayInfo.java)
//
// Tracks primary monitor properties and the full list of active monitors.
// All getters perform lazy discovery on first invocation.

i32         DisplayInfo_getMonitorCount();
fn            DisplayInfo_setMonitorCount(i32 val);

DisplayMonitor *DisplayInfo_getPrimaryMonitor();
fn            DisplayInfo_setPrimaryMonitor(DisplayMonitor *val);

DisplayMonitor *DisplayInfo_getMonitor(bytes index);
DisplayMonitor **DisplayInfo_getMonitors(bytes *outCount);
fn            DisplayInfo_setMonitors(DisplayMonitor **monitors, bytes count);

// Primary Monitor Active Physical Pixel Resolution
i32         DisplayInfo_getMonitorResolutionWidth();
fn            DisplayInfo_setMonitorResolutionWidth(i32 val);

i32         DisplayInfo_getMonitorResolutionHeight();
fn            DisplayInfo_setMonitorResolutionHeight(i32 val);

// Primary Monitor Logical Point Resolution (AppKit / UI space)
i32         DisplayInfo_getPointResolutionWidth();
fn            DisplayInfo_setPointResolutionWidth(i32 val);

i32         DisplayInfo_getPointResolutionHeight();
fn            DisplayInfo_setPointResolutionHeight(i32 val);

// Primary Monitor Native Hardware Panel Resolution
i32         DisplayInfo_getNativeResolutionWidth();
fn            DisplayInfo_setNativeResolutionWidth(i32 val);

i32         DisplayInfo_getNativeResolutionHeight();
fn            DisplayInfo_setNativeResolutionHeight(i32 val);

i32         DisplayInfo_getCurrentRefreshRate();
fn            DisplayInfo_setCurrentRefreshRate(i32 val);

bool            DisplayInfo_getHdrSupported();
fn            DisplayInfo_setHdrSupported(bool val);

f32           DisplayInfo_getDisplayDensity();
fn            DisplayInfo_setDisplayDensity(f32 val);

// Native Hardware Panel Density (Native Panel Width / Point Width)
f32           DisplayInfo_getHardwareDensity();
fn            DisplayInfo_setHardwareDensity(f32 val);

#endif
