#include "vex.h"
#include "system/discovery.h"

#include "system/display_info.h"
#include "system/display_monitor.h"
#include "system/hardware_info.h"
#include "system/graphics_info.h"

#if defined(VEX_PLATFORM_APPLE)
#include <sys/sysctl.h>
#include <CoreGraphics/CoreGraphics.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Discovery
 * ============================================================================
 * Native hardware, display, and GPU probe (Legacy: system/SystemDiscovery.java).
 * Bootstraps exactly once under an atomic flag, then writes sysctl hardware
 * facts, IOKit battery state, the Metal device, and CoreGraphics display
 * modes into the HardwareInfo / DisplayInfo / GraphicsInfo singletons.
 * Procedural: no own struct — the only state is the static atomic
 * s_bootstrapped flag; refresh clears it and re-runs the probe.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Discovery (system/discovery.m)
 * ============================================================================
 * native hardware, display, and GPU probe (Legacy: system/SystemDiscovery.java)
 *
 * STRUCT FIELDS: none — procedural (operates on HardwareInfo/DisplayInfo/
 * GraphicsInfo singletons; only state is the static atomic s_bootstrapped flag)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - SystemDiscovery_bootstrap()
 *   - SystemDiscovery_refresh()
 *
 * Getters:
 *   - SystemDiscovery_isBootstrapped()
 * ============================================================================
 */

#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/ps/IOPSKeys.h>
#endif

static _Atomic bool s_bootstrapped = false;

bool SystemDiscovery_isBootstrapped() {
    return atomic_load_explicit(&s_bootstrapped, memory_order_acquire);
}

fn SystemDiscovery_refresh() {
    atomic_store_explicit(&s_bootstrapped, false, memory_order_release);
    SystemDiscovery_bootstrap();
}

fn SystemDiscovery_bootstrap() {
    bool expected = false;
    if (!atomic_compare_exchange_strong(&s_bootstrapped, &expected, true))
        return; // already bootstrapped

#if defined(VEX_PLATFORM_APPLE)
    @autoreleasepool {
        // --- 1. Hardware Discovery (sysctl) ---
        u64 memsize = 0;
        bytes len = sizeof(memsize);
        if (sysctlbyname("hw.memsize", &memsize, &len, nullptr, 0) == 0) {
            HardwareInfo_setRamTotal(memsize);
            HardwareInfo_setRamAvailable(memsize);
        }

        int cpuThreads = 0;
        len = sizeof(cpuThreads);
        if (sysctlbyname("hw.logicalcpu", &cpuThreads, &len, nullptr, 0) == 0) {
            HardwareInfo_setCpuThreadCount(cpuThreads);
        }

        int cpuCores = 0;
        len = sizeof(cpuCores);
        if (sysctlbyname("hw.physicalcpu", &cpuCores, &len, nullptr, 0) == 0) {
            HardwareInfo_setCpuCoreCount(cpuCores);
        }

        char model[128] = {0};
        len = sizeof(model) - 1;
        if (sysctlbyname("hw.model", model, &len, nullptr, 0) == 0) {
            HardwareInfo_setDeviceModel(model);
        }

        char brand[128] = {0};
        len = sizeof(brand) - 1;
        if (sysctlbyname("machdep.cpu.brand_string", brand, &len, nullptr, 0) == 0) {
            HardwareInfo_setCpuBrand(brand);
        } else if (model[0] != '\0') {
            HardwareInfo_setCpuBrand(model);
        }

        // --- 1b. Battery Discovery (IOKit Power Sources) ---
        CFTypeRef psInfo = IOPSCopyPowerSourcesInfo();
        if (psInfo) {
            CFArrayRef psList = IOPSCopyPowerSourcesList(psInfo);
            if (psList && CFArrayGetCount(psList) > 0) {
                HardwareInfo_setHasBattery(true);
                CFDictionaryRef desc = IOPSGetPowerSourceDescription(psInfo, CFArrayGetValueAtIndex(psList, 0));
                if (desc) {
                    NSNumber *curCap = (__bridge NSNumber*) CFDictionaryGetValue(desc, CFSTR(kIOPSCurrentCapacityKey));
                    NSNumber *maxCap = (__bridge NSNumber*) CFDictionaryGetValue(desc, CFSTR(kIOPSMaxCapacityKey));
                    NSString *state = (__bridge NSString*) CFDictionaryGetValue(desc, CFSTR(kIOPSPowerSourceStateKey));
                    NSNumber *charging = (__bridge NSNumber*) CFDictionaryGetValue(desc, CFSTR(kIOPSIsChargingKey));

                    if (curCap && maxCap && [maxCap f32Value] > 0.0f) {
                        HardwareInfo_setBatteryLevel([curCap f32Value] / [maxCap f32Value]);
                    }
                    if ([charging boolValue]) {
                        HardwareInfo_setBatteryStatus("Charging");
                    } else if ([state isEqualToString:@kIOPSACPowerValue]) {
                        HardwareInfo_setBatteryStatus("AC Connected");
                    } else {
                        HardwareInfo_setBatteryStatus("Discharging");
                    }
                }
                CFRelease(psList);
            } else {
                // Desktop machine (Mac mini, Mac Studio, Mac Pro, iMac, Desktop PC)
                HardwareInfo_setHasBattery(false);
                HardwareInfo_setBatteryLevel(-1.0f);
                HardwareInfo_setBatteryStatus("None (Desktop / AC)");
                if (psList) CFRelease(psList);
            }
            CFRelease(psInfo);
        } else {
            HardwareInfo_setHasBattery(false);
            HardwareInfo_setBatteryLevel(-1.0f);
            HardwareInfo_setBatteryStatus("None (Desktop / AC)");
        }

        // --- 2. Graphics Discovery (Metal / Apple Silicon) ---
        id<MTLDevice> defaultDevice = MTLCreateSystemDefaultDevice();
        if (defaultDevice) {
            GraphicsInfo_setGpuName([[defaultDevice name] UTF8String]);
            GraphicsInfo_setUnifiedMemoryEnabled([defaultDevice hasUnifiedMemory]);
            GraphicsInfo_setVramTotal([defaultDevice recommendedMaxWorkingSetSize]);
            GraphicsInfo_setPrimaryGraphicsApi("Metal 3 / Vulkan 1.3");
        }

        // --- 3. Display Discovery (CoreGraphics Displays) ---
        CGDirectDisplayID displayList[16];
        u32 displayCount = 0;
        if (CGGetActiveDisplayList(16, displayList, &displayCount) == kCGErrorSuccess && displayCount > 0) {
            CGDirectDisplayID mainDisplayId = CGMainDisplayID();
            DisplayMonitor *monitors[16] = {0};
            DisplayMonitor *primary = nullptr;

            for (u32 i = 0; i < displayCount; i++) {
                CGDirectDisplayID dId = displayList[i];
                DisplayMonitor *m = DisplayMonitor();
                if (!m) continue;

                DisplayMonitor_setId(m, (u32)dId);

                // Current Display Mode
                CGDisplayModeRef mode = CGDisplayCopyDisplayMode(dId);
                i32 curPixelW = 0, curPixelH = 0;
                i32 curPointW = 0, curPointH = 0;
                i32 refreshHz = 60;

                if (mode) {
                    curPixelW = (i32)CGDisplayModeGetPixelWidth(mode);
                    curPixelH = (i32)CGDisplayModeGetPixelHeight(mode);
                    curPointW = (i32)CGDisplayModeGetWidth(mode);
                    curPointH = (i32)CGDisplayModeGetHeight(mode);
                    d64 hz = CGDisplayModeGetRefreshRate(mode);
                    if (hz > 0.0) refreshHz = (i32)hz;
                    CGDisplayModeRelease(mode);
                }

                DisplayMonitor_setCurrentWidth(m, curPixelW);
                DisplayMonitor_setCurrentHeight(m, curPixelH);
                DisplayMonitor_setPointWidth(m, curPointW);
                DisplayMonitor_setPointHeight(m, curPointH);
                DisplayMonitor_setRefreshRate(m, refreshHz);

                f32 density = (curPointW > 0 && curPixelW > 0)
                                    ? ((f32)curPixelW / (f32)curPointW)
                                    : 1.0f;
                DisplayMonitor_setDpi(m, density);

                // Native Hardware Panel Resolution (look for kDisplayModeNativeFlag = 0x02000000)
                CFArrayRef allModes = CGDisplayCopyAllDisplayModes(dId, nullptr);
                i32 nativeW = curPixelW;
                i32 nativeH = curPixelH;
                bool foundNative = false;

                if (allModes) {
                    CFIndex modeCount = CFArrayGetCount(allModes);
                    for (CFIndex mIdx = 0; mIdx < modeCount; mIdx++) {
                        CGDisplayModeRef mRef = (CGDisplayModeRef)CFArrayGetValueAtIndex(allModes, mIdx);
                        u32 flags = CGDisplayModeGetIOFlags(mRef);
                        i32 mw = (i32)CGDisplayModeGetPixelWidth(mRef);
                        i32 mh = (i32)CGDisplayModeGetPixelHeight(mRef);

                        // 0x02000000 is kDisplayModeNativeFlag in IOKit/CoreGraphics
                        if ((flags & 0x02000000) != 0) {
                            nativeW = mw;
                            nativeH = mh;
                            foundNative = true;
                            break;
                        }
                    }

                    // Fallback: look for default mode or max resolution
                    if (!foundNative) {
                        for (CFIndex mIdx = 0; mIdx < modeCount; mIdx++) {
                            CGDisplayModeRef mRef = (CGDisplayModeRef)CFArrayGetValueAtIndex(allModes, mIdx);
                            u32 flags = CGDisplayModeGetIOFlags(mRef);
                            i32 mw = (i32)CGDisplayModeGetPixelWidth(mRef);
                            i32 mh = (i32)CGDisplayModeGetPixelHeight(mRef);
                            if ((flags & 0x00000001) != 0 || (mw * mh > nativeW * nativeH)) {
                                nativeW = mw;
                                nativeH = mh;
                            }
                        }
                    }
                    CFRelease(allModes);
                }

                DisplayMonitor_setNativeWidth(m, nativeW);
                DisplayMonitor_setNativeHeight(m, nativeH);

                // Name & Built-in detection
                char nameBuf[128];
                if (CGDisplayIsBuiltin(dId)) {
                    snprintf(nameBuf, sizeof(nameBuf), "Built-in Retina Display");
                } else {
                    snprintf(nameBuf, sizeof(nameBuf), "External Display %u", (unsigned int)dId);
                }
                DisplayMonitor_setName(m, nameBuf);

                // HDR Support check
                bool hdr = false;
                for (NSScreen *screen in [NSScreen screens]) {
                    NSDictionary *deviceDesc = [screen deviceDescription];
                    NSNumber *screenNumber = [deviceDesc objectForKey:@"NSScreenNumber"];
                    if (screenNumber && [screenNumber unsignedIntValue] == dId) {
                        if ([screen maximumExtendedDynamicRangeColorComponentValue] > 1.0)
                            hdr = true;
                        break;
                    }
                }
                DisplayMonitor_setHdrSupported(m, hdr);

                monitors[i] = m;
                if (dId == mainDisplayId) {
                    primary = m;
                }
            }

            if (!primary && displayCount > 0)
                primary = monitors[0];

            DisplayInfo_setMonitors(monitors, displayCount);
            DisplayInfo_setPrimaryMonitor(primary);

            if (primary) {
                DisplayInfo_setMonitorResolutionWidth(DisplayMonitor_getCurrentWidth(primary));
                DisplayInfo_setMonitorResolutionHeight(DisplayMonitor_getCurrentHeight(primary));
                DisplayInfo_setPointResolutionWidth(DisplayMonitor_getPointWidth(primary));
                DisplayInfo_setPointResolutionHeight(DisplayMonitor_getPointHeight(primary));
                DisplayInfo_setNativeResolutionWidth(DisplayMonitor_getNativeWidth(primary));
                DisplayInfo_setNativeResolutionHeight(DisplayMonitor_getNativeHeight(primary));
                DisplayInfo_setCurrentRefreshRate(DisplayMonitor_getRefreshRate(primary));
                DisplayInfo_setDisplayDensity(DisplayMonitor_getDpi(primary));
                f32 hwDensity = (DisplayMonitor_getPointWidth(primary) > 0 && DisplayMonitor_getNativeWidth(primary) > 0)
                                      ? ((f32)DisplayMonitor_getNativeWidth(primary) / (f32)DisplayMonitor_getPointWidth(primary))
                                      : DisplayMonitor_getDpi(primary);
                DisplayInfo_setHardwareDensity(hwDensity);
                DisplayInfo_setHdrSupported(DisplayMonitor_getHdrSupported(primary));
            }
        }
    }
#else
    // Portable fallback
    HardwareInfo_setCpuCoreCount(4);
    HardwareInfo_setCpuThreadCount(8);
    HardwareInfo_setRamTotal(8ULL * 1024 * 1024 * 1024);
    DisplayInfo_setMonitorResolutionWidth(1920);
    DisplayInfo_setMonitorResolutionHeight(1080);
    DisplayInfo_setPointResolutionWidth(1920);
    DisplayInfo_setPointResolutionHeight(1080);
    DisplayInfo_setNativeResolutionWidth(1920);
    DisplayInfo_setNativeResolutionHeight(1080);
    DisplayInfo_setCurrentRefreshRate(60);
    DisplayInfo_setDisplayDensity(1.0f);
#endif
}
