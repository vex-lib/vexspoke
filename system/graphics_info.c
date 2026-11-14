#include "vex.h"
#include "system/graphics_info.h"

#include "system/discovery.h"

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
 *   - GraphicsInfo_getGpuName()
 *   - GraphicsInfo_getGpuVendorId()
 *   - GraphicsInfo_getGpuDeviceId()
 *   - GraphicsInfo_getPrimaryGraphicsApi()
 *   - GraphicsInfo_getUnifiedMemoryEnabled()
 *   - GraphicsInfo_getComputeShadersEnabled()
 *   - GraphicsInfo_getMeshShadersEnabled()
 *   - GraphicsInfo_getHardwareRayTracingEnabled()
 *   - GraphicsInfo_getMaxTextureSize()
 *   - GraphicsInfo_getVramTotal()
 *   - GraphicsInfo_getVramAvailable()
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

static inline fn ensureDiscovered() {
    SystemDiscovery_bootstrap();
}

const char *GraphicsInfo_getGpuName() {
    ensureDiscovered();
    return s_gpuName;
}

fn GraphicsInfo_setGpuName(const char *val) {
    if (val) {
        strncpy(s_gpuName, val, sizeof(s_gpuName) - 1);
        s_gpuName[sizeof(s_gpuName) - 1] = '\0';
    }
}

i32 GraphicsInfo_getGpuVendorId() {
    ensureDiscovered();
    return s_gpuVendorId;
}

fn GraphicsInfo_setGpuVendorId(i32 val) {
    s_gpuVendorId = val;
}

i32 GraphicsInfo_getGpuDeviceId() {
    ensureDiscovered();
    return s_gpuDeviceId;
}

fn GraphicsInfo_setGpuDeviceId(i32 val) {
    s_gpuDeviceId = val;
}

const char *GraphicsInfo_getPrimaryGraphicsApi() {
    ensureDiscovered();
    return s_primaryGraphicsApi;
}

fn GraphicsInfo_setPrimaryGraphicsApi(const char *val) {
    if (val) {
        strncpy(s_primaryGraphicsApi, val, sizeof(s_primaryGraphicsApi) - 1);
        s_primaryGraphicsApi[sizeof(s_primaryGraphicsApi) - 1] = '\0';
    }
}

bool GraphicsInfo_getUnifiedMemoryEnabled() {
    ensureDiscovered();
    return s_unifiedMemoryEnabled;
}

fn GraphicsInfo_setUnifiedMemoryEnabled(bool val) {
    s_unifiedMemoryEnabled = val;
}

bool GraphicsInfo_getComputeShadersEnabled() {
    ensureDiscovered();
    return s_computeShadersEnabled;
}

fn GraphicsInfo_setComputeShadersEnabled(bool val) {
    s_computeShadersEnabled = val;
}

bool GraphicsInfo_getMeshShadersEnabled() {
    ensureDiscovered();
    return s_meshShadersEnabled;
}

fn GraphicsInfo_setMeshShadersEnabled(bool val) {
    s_meshShadersEnabled = val;
}

bool GraphicsInfo_getHardwareRayTracingEnabled() {
    ensureDiscovered();
    return s_hardwareRayTracingEnabled;
}

fn GraphicsInfo_setHardwareRayTracingEnabled(bool val) {
    s_hardwareRayTracingEnabled = val;
}

i32 GraphicsInfo_getMaxTextureSize() {
    ensureDiscovered();
    return s_maxTextureSize;
}

fn GraphicsInfo_setMaxTextureSize(i32 val) {
    s_maxTextureSize = val;
}

u64 GraphicsInfo_getVramTotal() {
    ensureDiscovered();
    return s_vramTotal;
}

fn GraphicsInfo_setVramTotal(u64 val) {
    s_vramTotal = val;
}

u64 GraphicsInfo_getVramAvailable() {
    ensureDiscovered();
    return s_vramAvailable;
}

fn GraphicsInfo_setVramAvailable(u64 val) {
    s_vramAvailable = val;
}
