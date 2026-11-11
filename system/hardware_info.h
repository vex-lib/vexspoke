#ifndef SYSTEM_HARDWARE_INFO_H
#define SYSTEM_HARDWARE_INFO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// system/hardware_info.h — host hardware information query (Legacy: system/HardwareInfo.java)

const char *HardwareInfo_getOperatingSystem(void);
void        HardwareInfo_setOperatingSystem(const char *val);

const char *HardwareInfo_getSystemArchitecture(void);
void        HardwareInfo_setSystemArchitecture(const char *val);

const char *HardwareInfo_getDeviceModel(void);
void        HardwareInfo_setDeviceModel(const char *val);

const char *HardwareInfo_getCpuBrand(void);
void        HardwareInfo_setCpuBrand(const char *val);

i32     HardwareInfo_getCpuCoreCount(void);
void        HardwareInfo_setCpuCoreCount(i32 val);

i32     HardwareInfo_getCpuThreadCount(void);
void        HardwareInfo_setCpuThreadCount(i32 val);

u64    HardwareInfo_getRamTotal(void);
void        HardwareInfo_setRamTotal(u64 val);

u64    HardwareInfo_getRamAvailable(void);
void        HardwareInfo_setRamAvailable(u64 val);

u64    HardwareInfo_getStorageTotalSpace(void);
void        HardwareInfo_setStorageTotalSpace(u64 val);

u64    HardwareInfo_getStorageAvailableSpace(void);
void        HardwareInfo_setStorageAvailableSpace(u64 val);

bool        HardwareInfo_hasBattery(void);
void        HardwareInfo_setHasBattery(bool val);

f32       HardwareInfo_getBatteryLevel(void);
void        HardwareInfo_setBatteryLevel(f32 val);

const char *HardwareInfo_getBatteryStatus(void);
void        HardwareInfo_setBatteryStatus(const char *val);

#endif
