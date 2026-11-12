#ifndef SYSTEM_DISPLAY_MONITOR_H
#define SYSTEM_DISPLAY_MONITOR_H

#include "vex.h"

// system/display_monitor.h — single display monitor representation (Legacy: system/DisplayMonitor.java)
//
// Represents a physical or virtual display with both its logical point resolution (AppKit/UI coordinates)
// and its physical pixel framebuffer resolution (backing swapchain / panel hardware pixels).

typedef struct DisplayMonitor DisplayMonitor;

DisplayMonitor *DisplayMonitor_0(void);
void            DisplayMonitor_free(DisplayMonitor *monitor);

u32        DisplayMonitor_getId(const DisplayMonitor *m);
void            DisplayMonitor_setId(DisplayMonitor *m, u32 id);

const char     *DisplayMonitor_getName(const DisplayMonitor *m);
void            DisplayMonitor_setName(DisplayMonitor *m, const char *name);

// Active Physical Pixel Resolution (actual framebuffer pixels)
i32         DisplayMonitor_getCurrentWidth(const DisplayMonitor *m);
void            DisplayMonitor_setCurrentWidth(DisplayMonitor *m, i32 val);

i32         DisplayMonitor_getCurrentHeight(const DisplayMonitor *m);
void            DisplayMonitor_setCurrentHeight(DisplayMonitor *m, i32 val);

// Logical Point Resolution (AppKit / UI layout coordinates)
i32         DisplayMonitor_getPointWidth(const DisplayMonitor *m);
void            DisplayMonitor_setPointWidth(DisplayMonitor *m, i32 val);

i32         DisplayMonitor_getPointHeight(const DisplayMonitor *m);
void            DisplayMonitor_setPointHeight(DisplayMonitor *m, i32 val);

// Native Physical Panel Resolution (hardware panel pixel grid)
i32         DisplayMonitor_getNativeWidth(const DisplayMonitor *m);
void            DisplayMonitor_setNativeWidth(DisplayMonitor *m, i32 val);

i32         DisplayMonitor_getNativeHeight(const DisplayMonitor *m);
void            DisplayMonitor_setNativeHeight(DisplayMonitor *m, i32 val);

i32         DisplayMonitor_getRefreshRate(const DisplayMonitor *m);
void            DisplayMonitor_setRefreshRate(DisplayMonitor *m, i32 val);

bool            DisplayMonitor_getHdrSupported(const DisplayMonitor *m);
void            DisplayMonitor_setHdrSupported(DisplayMonitor *m, bool val);

f32           DisplayMonitor_getDpi(const DisplayMonitor *m);
void            DisplayMonitor_setDpi(DisplayMonitor *m, f32 val);


#define DisplayMonitor(...) CONSTRUCTOR_DISPATCH(DisplayMonitor, __VA_ARGS__)
#endif
