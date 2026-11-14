// vexspoke/test/system/graphics_info_test.c — owner test for
// system/graphics_info.
//
// Proves the GPU capability snapshot: lazy bootstrap yields a non-empty GPU
// name and a non-negative max texture size, and every setter/getter round
// trips (name, vendor/device ids, API string, capability flags, max texture
// size, VRAM totals). A nullptr string setter is a no-op.

#include "vex.h"
#include <stdio.h>
#include <string.h>

#include "system/graphics_info.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Verifies graphics capability getters/setters and preserves strings on null updates.
// Exercises graphics-adapter information queries and returned metadata.
// Checks graphics capability snapshot accessors and setter/getter behavior.
int main() {
    const char *name = GraphicsInfo_getGpuName();
    const char *api = GraphicsInfo_getPrimaryGraphicsApi();
    CHECK(name != nullptr && name[0] != '\0');
    CHECK(api != nullptr);
    CHECK(GraphicsInfo_getMaxTextureSize() >= 0);

    GraphicsInfo_setGpuName("Test GPU");
    CHECK(strcmp(GraphicsInfo_getGpuName(), "Test GPU") == 0);
    GraphicsInfo_setPrimaryGraphicsApi("TestAPI");
    CHECK(strcmp(GraphicsInfo_getPrimaryGraphicsApi(), "TestAPI") == 0);

    GraphicsInfo_setGpuVendorId(0x106B);
    CHECK(GraphicsInfo_getGpuVendorId() == 0x106B);
    GraphicsInfo_setGpuDeviceId(42);
    CHECK(GraphicsInfo_getGpuDeviceId() == 42);

    GraphicsInfo_setUnifiedMemoryEnabled(false);
    CHECK(!GraphicsInfo_getUnifiedMemoryEnabled());
    GraphicsInfo_setUnifiedMemoryEnabled(true);
    CHECK(GraphicsInfo_getUnifiedMemoryEnabled());

    GraphicsInfo_setComputeShadersEnabled(false);
    CHECK(!GraphicsInfo_getComputeShadersEnabled());
    GraphicsInfo_setMeshShadersEnabled(false);
    CHECK(!GraphicsInfo_getMeshShadersEnabled());
    GraphicsInfo_setHardwareRayTracingEnabled(false);
    CHECK(!GraphicsInfo_getHardwareRayTracingEnabled());

    GraphicsInfo_setMaxTextureSize(8192);
    CHECK(GraphicsInfo_getMaxTextureSize() == 8192);
    GraphicsInfo_setVramTotal(1ull << 34);
    CHECK(GraphicsInfo_getVramTotal() == (1ull << 34));
    GraphicsInfo_setVramAvailable(1ull << 33);
    CHECK(GraphicsInfo_getVramAvailable() == (1ull << 33));

    GraphicsInfo_setGpuName(nullptr);
    CHECK(strcmp(GraphicsInfo_getGpuName(), "Test GPU") == 0);
    GraphicsInfo_setPrimaryGraphicsApi(nullptr);
    CHECK(strcmp(GraphicsInfo_getPrimaryGraphicsApi(), "TestAPI") == 0);

    if (g_failures == 0) {
        printf("graphics_info_test: all assertions held\n");
        return 0;
    }
    printf("graphics_info_test: %d FAILURES\n", g_failures);
    return 1;
}
