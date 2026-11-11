// vexspoke/test/system/display_info_test.c — owner test for
// system/display_info.
//
// Proves the global display registry:
//   - lazy bootstrap runs without crashing;
//   - monitor list set/get with bounds: getMonitor(index) returns null past the
//     count, getMonitors reports the count and returns the array;
//   - primary monitor pointer round trips;
//   - every resolution/refresh/density/HDR scalar round trips.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "system/display_info.h"
#include "system/display_monitor.h"
#include "nio/mem.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Tests display registry overrides, monitor bounds, primary selection, and scalar properties.
// Checks display resolution, scale, refresh, and capability queries.
// Checks monitor registry access and DisplayInfo scalar/pointer round trips.
int main(void) {
    CHECK(Memory_init(0));

    // Force bootstrap before overriding anything.
    (void) DisplayInfo_getMonitorCount();
    (void) DisplayInfo_getPrimaryMonitor();

    // No monitors registered past the count.
    CHECK(DisplayInfo_getMonitor(9999) == nullptr);

    DisplayMonitor *a = DisplayMonitor_0();
    DisplayMonitor *b = DisplayMonitor_0();
    CHECK(a != nullptr && b != nullptr);

    DisplayMonitor *list[2] = { a, b };
    DisplayInfo_setMonitors(list, 2);
    CHECK(DisplayInfo_getMonitorCount() == 2);
    CHECK(DisplayInfo_getMonitor(0) == a);
    CHECK(DisplayInfo_getMonitor(1) == b);
    CHECK(DisplayInfo_getMonitor(2) == nullptr);       // one past the end

    bytes count = 0;
    DisplayMonitor **all = DisplayInfo_getMonitors(&count);
    CHECK(count == 2);
    CHECK(all != nullptr && all[0] == a && all[1] == b);
    DisplayInfo_getMonitors(nullptr);                  // null out is safe

    DisplayInfo_setPrimaryMonitor(a);
    CHECK(DisplayInfo_getPrimaryMonitor() == a);
    DisplayInfo_setPrimaryMonitor(nullptr);
    CHECK(DisplayInfo_getPrimaryMonitor() == nullptr);

    DisplayInfo_setMonitorResolutionWidth(3456);
    DisplayInfo_setMonitorResolutionHeight(2234);
    CHECK(DisplayInfo_getMonitorResolutionWidth() == 3456);
    CHECK(DisplayInfo_getMonitorResolutionHeight() == 2234);

    DisplayInfo_setPointResolutionWidth(1728);
    DisplayInfo_setPointResolutionHeight(1117);
    CHECK(DisplayInfo_getPointResolutionWidth() == 1728);
    CHECK(DisplayInfo_getPointResolutionHeight() == 1117);

    DisplayInfo_setNativeResolutionWidth(3456);
    DisplayInfo_setNativeResolutionHeight(2234);
    CHECK(DisplayInfo_getNativeResolutionWidth() == 3456);
    CHECK(DisplayInfo_getNativeResolutionHeight() == 2234);

    DisplayInfo_setCurrentRefreshRate(120);
    CHECK(DisplayInfo_getCurrentRefreshRate() == 120);
    DisplayInfo_setHdrSupported(true);
    CHECK(DisplayInfo_getHdrSupported());
    DisplayInfo_setDisplayDensity(2.0f);
    CHECK(DisplayInfo_getDisplayDensity() == 2.0f);
    DisplayInfo_setHardwareDensity(2.0f);
    CHECK(DisplayInfo_getHardwareDensity() == 2.0f);

    DisplayMonitor_free(a);
    DisplayMonitor_free(b);

    if (g_failures == 0) {
        printf("display_info_test: all assertions held\n");
        return 0;
    }
    printf("display_info_test: %d FAILURES\n", g_failures);
    return 1;
}
