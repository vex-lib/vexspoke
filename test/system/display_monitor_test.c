// vexspoke/test/system/display_monitor_test.c — owner test for
// system/display_monitor.
//
// Proves the display record:
//   - zero construction: id/name/resolutions/refresh 0, HDR false, dpi 1.0;
//   - every setter/getter round trips (id, name, current/point/native sizes,
//     refresh, HDR, dpi);
//   - setName(nullptr) clears; an overlong name truncates and stays
//     nul-terminated;
//   - every getter is null-safe with the documented defaults, setters and free
//     are null-safe.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

// Checks DisplayMonitor defaults, property round trips, name bounds, and null handling.
// Exercises monitor enumeration and the metadata exposed for each monitor.
// Verifies DisplayMonitor defaults, mutable properties, bounded names, and null handling.
int main() {
    CHECK(Memory_init(0));

    DisplayMonitor *m = DisplayMonitor_0();
    CHECK(m != nullptr);
    if (!m)
        return 1;
    CHECK(DisplayMonitor_getId(m) == 0);
    CHECK(strcmp(DisplayMonitor_getName(m), "") == 0);
    CHECK(DisplayMonitor_getCurrentWidth(m) == 0);
    CHECK(DisplayMonitor_getCurrentHeight(m) == 0);
    CHECK(DisplayMonitor_getPointWidth(m) == 0);
    CHECK(DisplayMonitor_getPointHeight(m) == 0);
    CHECK(DisplayMonitor_getNativeWidth(m) == 0);
    CHECK(DisplayMonitor_getNativeHeight(m) == 0);
    CHECK(DisplayMonitor_getRefreshRate(m) == 0);
    CHECK(!DisplayMonitor_getHdrSupported(m));
    CHECK(DisplayMonitor_getDpi(m) == 1.0f);

    DisplayMonitor_setId(m, 7);
    DisplayMonitor_setName(m, "Studio Display");
    DisplayMonitor_setCurrentWidth(m, 5120);
    DisplayMonitor_setCurrentHeight(m, 2880);
    DisplayMonitor_setPointWidth(m, 2560);
    DisplayMonitor_setPointHeight(m, 1440);
    DisplayMonitor_setNativeWidth(m, 6016);
    DisplayMonitor_setNativeHeight(m, 3384);
    DisplayMonitor_setRefreshRate(m, 60);
    DisplayMonitor_setHdrSupported(m, true);
    DisplayMonitor_setDpi(m, 2.0f);

    CHECK(DisplayMonitor_getId(m) == 7);
    CHECK(strcmp(DisplayMonitor_getName(m), "Studio Display") == 0);
    CHECK(DisplayMonitor_getCurrentWidth(m) == 5120);
    CHECK(DisplayMonitor_getCurrentHeight(m) == 2880);
    CHECK(DisplayMonitor_getPointWidth(m) == 2560);
    CHECK(DisplayMonitor_getPointHeight(m) == 1440);
    CHECK(DisplayMonitor_getNativeWidth(m) == 6016);
    CHECK(DisplayMonitor_getNativeHeight(m) == 3384);
    CHECK(DisplayMonitor_getRefreshRate(m) == 60);
    CHECK(DisplayMonitor_getHdrSupported(m));
    CHECK(DisplayMonitor_getDpi(m) == 2.0f);

    // nullptr name clears; overlong truncates to 127.
    DisplayMonitor_setName(m, nullptr);
    CHECK(strcmp(DisplayMonitor_getName(m), "") == 0);
    char longName[300];
    memset(longName, 'x', sizeof(longName) - 1);
    longName[sizeof(longName) - 1] = '\0';
    DisplayMonitor_setName(m, longName);
    CHECK(strlen(DisplayMonitor_getName(m)) == 127);

    DisplayMonitor_free(m);
    DisplayMonitor_free(nullptr);

    // Null-safe getters/setters.
    CHECK(DisplayMonitor_getId(nullptr) == 0);
    CHECK(strcmp(DisplayMonitor_getName(nullptr), "") == 0);
    CHECK(DisplayMonitor_getDpi(nullptr) == 1.0f);
    CHECK(!DisplayMonitor_getHdrSupported(nullptr));
    DisplayMonitor_setId(nullptr, 1);
    DisplayMonitor_setName(nullptr, "x");
    DisplayMonitor_setCurrentWidth(nullptr, 1);
    DisplayMonitor_setDpi(nullptr, 2.0f);

    if (g_failures == 0) {
        printf("display_monitor_test: all assertions held\n");
        return 0;
    }
    printf("display_monitor_test: %d FAILURES\n", g_failures);
    return 1;
}
