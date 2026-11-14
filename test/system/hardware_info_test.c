// vexspoke/test/system/hardware_info_test.c — owner test for
// system/hardware_info.
//
// Proves the host hardware snapshot:
//   - the first getter lazily bootstraps discovery without crashing, and
//     yields plausible real values (non-empty OS/arch, non-negative counts);
//   - every setter/getter round trips (strings, counts, RAM/storage totals,
//     battery flag/level/status);
//   - a nullptr string setter is a no-op (the previous value survives).

#include "vex.h"
#include <stdio.h>
#include <string.h>

#include "system/hardware_info.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Checks hardware discovery defaults, property round trips, and null-string preservation.
// Verifies hardware-information queries return platform data or safe defaults.
// Exercises lazy host discovery and HardwareInfo property round trips.
int main() {
    // Force the one-time lazy bootstrap.
    const char *os = HardwareInfo_getOperatingSystem();
    const char *arch = HardwareInfo_getSystemArchitecture();
    CHECK(os != nullptr && os[0] != '\0');
    CHECK(arch != nullptr && arch[0] != '\0');
    CHECK(HardwareInfo_getCpuCoreCount() >= 0);
    CHECK(HardwareInfo_getCpuThreadCount() >= 0);
    CHECK(HardwareInfo_getDeviceModel() != nullptr);
    CHECK(HardwareInfo_getCpuBrand() != nullptr);
    CHECK(HardwareInfo_getBatteryStatus() != nullptr);

    // Round trips after bootstrap (bootstrap runs once).
    HardwareInfo_setOperatingSystem("TestOS");
    CHECK(strcmp(HardwareInfo_getOperatingSystem(), "TestOS") == 0);
    HardwareInfo_setSystemArchitecture("wasm64");
    CHECK(strcmp(HardwareInfo_getSystemArchitecture(), "wasm64") == 0);
    HardwareInfo_setDeviceModel("TestModel");
    CHECK(strcmp(HardwareInfo_getDeviceModel(), "TestModel") == 0);
    HardwareInfo_setCpuBrand("TestCpu");
    CHECK(strcmp(HardwareInfo_getCpuBrand(), "TestCpu") == 0);

    HardwareInfo_setCpuCoreCount(12);
    CHECK(HardwareInfo_getCpuCoreCount() == 12);
    HardwareInfo_setCpuThreadCount(24);
    CHECK(HardwareInfo_getCpuThreadCount() == 24);
    HardwareInfo_setRamTotal(64ull * 1024 * 1024 * 1024);
    CHECK(HardwareInfo_getRamTotal() == 64ull * 1024 * 1024 * 1024);
    HardwareInfo_setRamAvailable(32ull * 1024 * 1024 * 1024);
    CHECK(HardwareInfo_getRamAvailable() == 32ull * 1024 * 1024 * 1024);
    HardwareInfo_setStorageTotalSpace(1000000000ull);
    CHECK(HardwareInfo_getStorageTotalSpace() == 1000000000ull);
    HardwareInfo_setStorageAvailableSpace(500000000ull);
    CHECK(HardwareInfo_getStorageAvailableSpace() == 500000000ull);

    HardwareInfo_setHasBattery(true);
    CHECK(HardwareInfo_hasBattery());
    HardwareInfo_setBatteryLevel(0.5f);
    CHECK(HardwareInfo_getBatteryLevel() == 0.5f);
    HardwareInfo_setBatteryStatus("Charging");
    CHECK(strcmp(HardwareInfo_getBatteryStatus(), "Charging") == 0);

    // A nullptr string setter keeps the current value.
    HardwareInfo_setOperatingSystem(nullptr);
    CHECK(strcmp(HardwareInfo_getOperatingSystem(), "TestOS") == 0);
    HardwareInfo_setBatteryStatus(nullptr);
    CHECK(strcmp(HardwareInfo_getBatteryStatus(), "Charging") == 0);
    HardwareInfo_setCpuBrand(nullptr);
    CHECK(strcmp(HardwareInfo_getCpuBrand(), "TestCpu") == 0);

    if (g_failures == 0) {
        printf("hardware_info_test: all assertions held\n");
        return 0;
    }
    printf("hardware_info_test: %d FAILURES\n", g_failures);
    return 1;
}
