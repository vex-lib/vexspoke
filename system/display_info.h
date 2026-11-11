#ifndef SYSTEM_DISPLAY_INFO_H
#define SYSTEM_DISPLAY_INFO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "system/display_monitor.h"

// system/display_info.h — global display subsystem query (Legacy: system/DisplayInfo.java)
//
// Tracks primary monitor properties and the full list of active monitors.
// All getters perform lazy discovery on first invocation.

i32         DisplayInfo_getMonitorCount(void);
void            DisplayInfo_setMonitorCount(i32 val);

DisplayMonitor *DisplayInfo_getPrimaryMonitor(void);
void            DisplayInfo_setPrimaryMonitor(DisplayMonitor *val);

DisplayMonitor *DisplayInfo_getMonitor(bytes index);
DisplayMonitor **DisplayInfo_getMonitors(bytes *outCount);
void            DisplayInfo_setMonitors(DisplayMonitor **monitors, bytes count);

// Primary Monitor Active Physical Pixel Resolution
i32         DisplayInfo_getMonitorResolutionWidth(void);
void            DisplayInfo_setMonitorResolutionWidth(i32 val);

i32         DisplayInfo_getMonitorResolutionHeight(void);
void            DisplayInfo_setMonitorResolutionHeight(i32 val);

// Primary Monitor Logical Point Resolution (AppKit / UI space)
i32         DisplayInfo_getPointResolutionWidth(void);
void            DisplayInfo_setPointResolutionWidth(i32 val);

i32         DisplayInfo_getPointResolutionHeight(void);
void            DisplayInfo_setPointResolutionHeight(i32 val);

// Primary Monitor Native Hardware Panel Resolution
i32         DisplayInfo_getNativeResolutionWidth(void);
void            DisplayInfo_setNativeResolutionWidth(i32 val);

i32         DisplayInfo_getNativeResolutionHeight(void);
void            DisplayInfo_setNativeResolutionHeight(i32 val);

i32         DisplayInfo_getCurrentRefreshRate(void);
void            DisplayInfo_setCurrentRefreshRate(i32 val);

bool            DisplayInfo_getHdrSupported(void);
void            DisplayInfo_setHdrSupported(bool val);

f32           DisplayInfo_getDisplayDensity(void);
void            DisplayInfo_setDisplayDensity(f32 val);

// Native Hardware Panel Density (Native Panel Width / Point Width)
f32           DisplayInfo_getHardwareDensity(void);
void            DisplayInfo_setHardwareDensity(f32 val);

#endif
