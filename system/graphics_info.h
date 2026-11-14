#ifndef SYSTEM_GRAPHICS_INFO_H
#define SYSTEM_GRAPHICS_INFO_H

#include "vex.h"

// system/graphics_info.h — GPU and graphics capabilities query (Legacy: system/GraphicsInfo.java)

const char *GraphicsInfo_getGpuName();
fn        GraphicsInfo_setGpuName(const char *val);

i32     GraphicsInfo_getGpuVendorId();
fn        GraphicsInfo_setGpuVendorId(i32 val);

i32     GraphicsInfo_getGpuDeviceId();
fn        GraphicsInfo_setGpuDeviceId(i32 val);

const char *GraphicsInfo_getPrimaryGraphicsApi();
fn        GraphicsInfo_setPrimaryGraphicsApi(const char *val);

bool        GraphicsInfo_getUnifiedMemoryEnabled();
fn        GraphicsInfo_setUnifiedMemoryEnabled(bool val);

bool        GraphicsInfo_getComputeShadersEnabled();
fn        GraphicsInfo_setComputeShadersEnabled(bool val);

bool        GraphicsInfo_getMeshShadersEnabled();
fn        GraphicsInfo_setMeshShadersEnabled(bool val);

bool        GraphicsInfo_getHardwareRayTracingEnabled();
fn        GraphicsInfo_setHardwareRayTracingEnabled(bool val);

i32     GraphicsInfo_getMaxTextureSize();
fn        GraphicsInfo_setMaxTextureSize(i32 val);

u64    GraphicsInfo_getVramTotal();
fn        GraphicsInfo_setVramTotal(u64 val);

u64    GraphicsInfo_getVramAvailable();
fn        GraphicsInfo_setVramAvailable(u64 val);

#endif
