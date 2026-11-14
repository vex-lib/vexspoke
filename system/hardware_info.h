#ifndef SYSTEM_HARDWARE_INFO_H
#define SYSTEM_HARDWARE_INFO_H

#include "vex.h"

// system/hardware_info.h — host hardware information query (Legacy: system/HardwareInfo.java)

const char *HardwareInfo_getOperatingSystem();
fn        HardwareInfo_setOperatingSystem(const char *val);

const char *HardwareInfo_getSystemArchitecture();
fn        HardwareInfo_setSystemArchitecture(const char *val);

const char *HardwareInfo_getDeviceModel();
fn        HardwareInfo_setDeviceModel(const char *val);

const char *HardwareInfo_getCpuBrand();
fn        HardwareInfo_setCpuBrand(const char *val);

i32     HardwareInfo_getCpuCoreCount();
fn        HardwareInfo_setCpuCoreCount(i32 val);

i32     HardwareInfo_getCpuThreadCount();
fn        HardwareInfo_setCpuThreadCount(i32 val);

u64    HardwareInfo_getRamTotal();
fn        HardwareInfo_setRamTotal(u64 val);

u64    HardwareInfo_getRamAvailable();
fn        HardwareInfo_setRamAvailable(u64 val);

u64    HardwareInfo_getStorageTotalSpace();
fn        HardwareInfo_setStorageTotalSpace(u64 val);

u64    HardwareInfo_getStorageAvailableSpace();
fn        HardwareInfo_setStorageAvailableSpace(u64 val);

bool        HardwareInfo_hasBattery();
fn        HardwareInfo_setHasBattery(bool val);

f32       HardwareInfo_getBatteryLevel();
fn        HardwareInfo_setBatteryLevel(f32 val);

const char *HardwareInfo_getBatteryStatus();
fn        HardwareInfo_setBatteryStatus(const char *val);

#endif
