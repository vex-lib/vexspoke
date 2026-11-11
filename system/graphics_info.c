#include "system/graphics_info.h"

#include <string.h>

#include "system/discovery.h"
#include "util/annotation.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Graphics_info
 * ============================================================================
 * GPU and graphics capabilities query (Legacy: system/GraphicsInfo.java): a
 * procedural class over static state — GPU name/vendor/device, primary
 * graphics API, unified-memory/compute/mesh/ray-tracing feature flags, max
 * texture size, and VRAM totals. Getters lazily trigger SystemDiscovery_
 * bootstrap once so the first query reflects the real device; setters let
 * callers override or stub the reported values. All state is process-global
 * and allocation-free.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Graphics_info (system/graphics_info.c)
 * ============================================================================
 * GPU and graphics capabilities query (Legacy: system/GraphicsInfo.java)
 *
 * STRUCT FIELDS: none — procedural (operates on graphics device query state)
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Setters:
 *   - GraphicsInfo_setGpuName(val)
 *   - GraphicsInfo_setGpuVendorId(val)
 *   - GraphicsInfo_setGpuDeviceId(val)
 *   - GraphicsInfo_setPrimaryGraphicsApi(val)
 *   - GraphicsInfo_setUnifiedMemoryEnabled(val)
 *   - GraphicsInfo_setComputeShadersEnabled(val)
 *   - GraphicsInfo_setMeshShadersEnabled(val)
 *   - GraphicsInfo_setHardwareRayTracingEnabled(val)
 *   - GraphicsInfo_setMaxTextureSize(val)
 *   - GraphicsInfo_setVramTotal(val)
 *   - GraphicsInfo_setVramAvailable(val)
 *
 * Getters:
 *   - GraphicsInfo_getGpuName(void)
 *   - GraphicsInfo_getGpuVendorId(void)
 *   - GraphicsInfo_getGpuDeviceId(void)
 *   - GraphicsInfo_getPrimaryGraphicsApi(void)
 *   - GraphicsInfo_getUnifiedMemoryEnabled(void)
 *   - GraphicsInfo_getComputeShadersEnabled(void)
 *   - GraphicsInfo_getMeshShadersEnabled(void)
 *   - GraphicsInfo_getHardwareRayTracingEnabled(void)
 *   - GraphicsInfo_getMaxTextureSize(void)
 *   - GraphicsInfo_getVramTotal(void)
 *   - GraphicsInfo_getVramAvailable(void)
 * ============================================================================
 */


static char     s_gpuName[128] = "Apple M-Series GPU";
static i32  s_gpuVendorId = 0x106B; // Apple vendor ID
static i32  s_gpuDeviceId = 0;
static char     s_primaryGraphicsApi[32] = "Metal/Vulkan";
static bool     s_unifiedMemoryEnabled = true;
static bool     s_computeShadersEnabled = true;
static bool     s_meshShadersEnabled = true;
static bool     s_hardwareRayTracingEnabled = true;
static i32  s_maxTextureSize = 16384;
static u64 s_vramTotal = 0;
static u64 s_vramAvailable = 0;

static inline void ensureDiscovered(void) {
    SystemDiscovery_bootstrap();
}

const char *GraphicsInfo_getGpuName(void) {
    ensureDiscovered();
    return s_gpuName;
}

void GraphicsInfo_setGpuName(const char *val) {
    if (val) {
        strncpy(s_gpuName, val, sizeof(s_gpuName) - 1);
        s_gpuName[sizeof(s_gpuName) - 1] = '\0';
    }
}

i32 GraphicsInfo_getGpuVendorId(void) {
    ensureDiscovered();
    return s_gpuVendorId;
}

void GraphicsInfo_setGpuVendorId(i32 val) {
    s_gpuVendorId = val;
}

i32 GraphicsInfo_getGpuDeviceId(void) {
    ensureDiscovered();
    return s_gpuDeviceId;
}

void GraphicsInfo_setGpuDeviceId(i32 val) {
    s_gpuDeviceId = val;
}

const char *GraphicsInfo_getPrimaryGraphicsApi(void) {
    ensureDiscovered();
    return s_primaryGraphicsApi;
}

void GraphicsInfo_setPrimaryGraphicsApi(const char *val) {
    if (val) {
        strncpy(s_primaryGraphicsApi, val, sizeof(s_primaryGraphicsApi) - 1);
        s_primaryGraphicsApi[sizeof(s_primaryGraphicsApi) - 1] = '\0';
    }
}

bool GraphicsInfo_getUnifiedMemoryEnabled(void) {
    ensureDiscovered();
    return s_unifiedMemoryEnabled;
}

void GraphicsInfo_setUnifiedMemoryEnabled(bool val) {
    s_unifiedMemoryEnabled = val;
}

bool GraphicsInfo_getComputeShadersEnabled(void) {
    ensureDiscovered();
    return s_computeShadersEnabled;
}

void GraphicsInfo_setComputeShadersEnabled(bool val) {
    s_computeShadersEnabled = val;
}

bool GraphicsInfo_getMeshShadersEnabled(void) {
    ensureDiscovered();
    return s_meshShadersEnabled;
}

void GraphicsInfo_setMeshShadersEnabled(bool val) {
    s_meshShadersEnabled = val;
}

bool GraphicsInfo_getHardwareRayTracingEnabled(void) {
    ensureDiscovered();
    return s_hardwareRayTracingEnabled;
}

void GraphicsInfo_setHardwareRayTracingEnabled(bool val) {
    s_hardwareRayTracingEnabled = val;
}

i32 GraphicsInfo_getMaxTextureSize(void) {
    ensureDiscovered();
    return s_maxTextureSize;
}

void GraphicsInfo_setMaxTextureSize(i32 val) {
    s_maxTextureSize = val;
}

u64 GraphicsInfo_getVramTotal(void) {
    ensureDiscovered();
    return s_vramTotal;
}

void GraphicsInfo_setVramTotal(u64 val) {
    s_vramTotal = val;
}

u64 GraphicsInfo_getVramAvailable(void) {
    ensureDiscovered();
    return s_vramAvailable;
}

void GraphicsInfo_setVramAvailable(u64 val) {
    s_vramAvailable = val;
}
