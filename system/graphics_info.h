#ifndef SYSTEM_GRAPHICS_INFO_H
#define SYSTEM_GRAPHICS_INFO_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// system/graphics_info.h — GPU and graphics capabilities query (Legacy: system/GraphicsInfo.java)

const char *GraphicsInfo_getGpuName(void);
void        GraphicsInfo_setGpuName(const char *val);

i32     GraphicsInfo_getGpuVendorId(void);
void        GraphicsInfo_setGpuVendorId(i32 val);

i32     GraphicsInfo_getGpuDeviceId(void);
void        GraphicsInfo_setGpuDeviceId(i32 val);

const char *GraphicsInfo_getPrimaryGraphicsApi(void);
void        GraphicsInfo_setPrimaryGraphicsApi(const char *val);

bool        GraphicsInfo_getUnifiedMemoryEnabled(void);
void        GraphicsInfo_setUnifiedMemoryEnabled(bool val);

bool        GraphicsInfo_getComputeShadersEnabled(void);
void        GraphicsInfo_setComputeShadersEnabled(bool val);

bool        GraphicsInfo_getMeshShadersEnabled(void);
void        GraphicsInfo_setMeshShadersEnabled(bool val);

bool        GraphicsInfo_getHardwareRayTracingEnabled(void);
void        GraphicsInfo_setHardwareRayTracingEnabled(bool val);

i32     GraphicsInfo_getMaxTextureSize(void);
void        GraphicsInfo_setMaxTextureSize(i32 val);

u64    GraphicsInfo_getVramTotal(void);
void        GraphicsInfo_setVramTotal(u64 val);

u64    GraphicsInfo_getVramAvailable(void);
void        GraphicsInfo_setVramAvailable(u64 val);

#endif
