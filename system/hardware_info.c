#include "vex.h"
#include "system/hardware_info.h"

#include "system/discovery.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: HardwareInfo
 * ============================================================================
 * Host hardware information query (Legacy: system/HardwareInfo.java): a
 * static snapshot of OS name, architecture, device model, CPU brand/core/
 * thread counts, RAM and storage totals/available, and battery state. Every
 * getter lazily triggers SystemDiscovery_bootstrap once so the snapshot is
 * populated before first read; setters allow test seams to override values.
 * Lives at R2 as a leaf system-query behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Hardware_info (system/hardware_info.c)
 * ============================================================================
 * host hardware information query (Legacy: system/HardwareInfo.java)
 *
 * STRUCT FIELDS: none — procedural (operates on hardware query state)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Setters:
 *   - HardwareInfo_setOperatingSystem(val)
 *   - HardwareInfo_setSystemArchitecture(val)
 *   - HardwareInfo_setDeviceModel(val)
 *   - HardwareInfo_setCpuBrand(val)
 *   - HardwareInfo_setCpuCoreCount(val)
 *   - HardwareInfo_setCpuThreadCount(val)
 *   - HardwareInfo_setRamTotal(val)
 *   - HardwareInfo_setRamAvailable(val)
 *   - HardwareInfo_setStorageTotalSpace(val)
 *   - HardwareInfo_setStorageAvailableSpace(val)
 *   - HardwareInfo_setHasBattery(val)
 *   - HardwareInfo_setBatteryLevel(val)
 *   - HardwareInfo_setBatteryStatus(val)
 *
 * Getters:
 *   - HardwareInfo_getOperatingSystem()
 *   - HardwareInfo_getSystemArchitecture()
 *   - HardwareInfo_getDeviceModel()
 *   - HardwareInfo_getCpuBrand()
 *   - HardwareInfo_getCpuCoreCount()
 *   - HardwareInfo_getCpuThreadCount()
 *   - HardwareInfo_getRamTotal()
 *   - HardwareInfo_getRamAvailable()
 *   - HardwareInfo_getStorageTotalSpace()
 *   - HardwareInfo_getStorageAvailableSpace()
 *   - HardwareInfo_hasBattery()
 *   - HardwareInfo_getBatteryLevel()
 *   - HardwareInfo_getBatteryStatus()
 * ============================================================================
 */


static char     s_operatingSystem[64] = "macOS";
static char     s_systemArchitecture[64] = "arm64";
static char     s_deviceModel[64] = "";
static char     s_cpuBrand[128] = "";
static i32  s_cpuCoreCount = 0;
static i32  s_cpuThreadCount = 0;
static u64 s_ramTotal = 0;
static u64 s_ramAvailable = 0;
static u64 s_storageTotalSpace = 0;
static u64 s_storageAvailableSpace = 0;
static f32    s_batteryLevel = -1.0f;
static bool     s_hasBattery = false;
static char     s_batteryStatus[32] = "None";

static inline fn ensureDiscovered() {
    SystemDiscovery_bootstrap();
}

bool HardwareInfo_hasBattery() {
    ensureDiscovered();
    return s_hasBattery;
}

fn HardwareInfo_setHasBattery(bool val) {
    s_hasBattery = val;
}

const char *HardwareInfo_getOperatingSystem() {
    ensureDiscovered();
    return s_operatingSystem;
}

fn HardwareInfo_setOperatingSystem(const char *val) {
    if (val) {
        strncpy(s_operatingSystem, val, sizeof(s_operatingSystem) - 1);
        s_operatingSystem[sizeof(s_operatingSystem) - 1] = '\0';
    }
}

const char *HardwareInfo_getSystemArchitecture() {
    ensureDiscovered();
    return s_systemArchitecture;
}

fn HardwareInfo_setSystemArchitecture(const char *val) {
    if (val) {
        strncpy(s_systemArchitecture, val, sizeof(s_systemArchitecture) - 1);
        s_systemArchitecture[sizeof(s_systemArchitecture) - 1] = '\0';
    }
}

const char *HardwareInfo_getDeviceModel() {
    ensureDiscovered();
    return s_deviceModel;
}

fn HardwareInfo_setDeviceModel(const char *val) {
    if (val) {
        strncpy(s_deviceModel, val, sizeof(s_deviceModel) - 1);
        s_deviceModel[sizeof(s_deviceModel) - 1] = '\0';
    }
}

const char *HardwareInfo_getCpuBrand() {
    ensureDiscovered();
    return s_cpuBrand;
}

fn HardwareInfo_setCpuBrand(const char *val) {
    if (val) {
        strncpy(s_cpuBrand, val, sizeof(s_cpuBrand) - 1);
        s_cpuBrand[sizeof(s_cpuBrand) - 1] = '\0';
    }
}

i32 HardwareInfo_getCpuCoreCount() {
    ensureDiscovered();
    return s_cpuCoreCount;
}

fn HardwareInfo_setCpuCoreCount(i32 val) {
    s_cpuCoreCount = val;
}

i32 HardwareInfo_getCpuThreadCount() {
    ensureDiscovered();
    return s_cpuThreadCount;
}

fn HardwareInfo_setCpuThreadCount(i32 val) {
    s_cpuThreadCount = val;
}

u64 HardwareInfo_getRamTotal() {
    ensureDiscovered();
    return s_ramTotal;
}

fn HardwareInfo_setRamTotal(u64 val) {
    s_ramTotal = val;
}

u64 HardwareInfo_getRamAvailable() {
    ensureDiscovered();
    return s_ramAvailable;
}

fn HardwareInfo_setRamAvailable(u64 val) {
    s_ramAvailable = val;
}

u64 HardwareInfo_getStorageTotalSpace() {
    ensureDiscovered();
    return s_storageTotalSpace;
}

fn HardwareInfo_setStorageTotalSpace(u64 val) {
    s_storageTotalSpace = val;
}

u64 HardwareInfo_getStorageAvailableSpace() {
    ensureDiscovered();
    return s_storageAvailableSpace;
}

fn HardwareInfo_setStorageAvailableSpace(u64 val) {
    s_storageAvailableSpace = val;
}

f32 HardwareInfo_getBatteryLevel() {
    ensureDiscovered();
    return s_batteryLevel;
}

fn HardwareInfo_setBatteryLevel(f32 val) {
    s_batteryLevel = val;
}

const char *HardwareInfo_getBatteryStatus() {
    ensureDiscovered();
    return s_batteryStatus;
}

fn HardwareInfo_setBatteryStatus(const char *val) {
    if (val) {
        strncpy(s_batteryStatus, val, sizeof(s_batteryStatus) - 1);
        s_batteryStatus[sizeof(s_batteryStatus) - 1] = '\0';
    }
}
