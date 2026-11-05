#include "annotation/annotation.h"
#include "system/capture_tool.h"
#include "system/process_probe.h"

#include <stdio.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: CaptureToolTest (capture_tool_test/test.c)
 * LEVEL: L2 — Behavior verification (headless; libproc probes, no
 * recording, no permission prompts)
 * ============================================================================
 * Executable proof of the capture directory: registry shape (count, slug
 * lookups, kind coverage, driver-vs-process row contract), live
 * liveness probes (self-truthy via ProcessProbe, false for fake names,
 * false for driver rows), driver dir scanning (absent dir = false), and
 * every null-safety guard.
 *
 * Environment-tolerant: capture tools' running state is REPORTED, never
 * asserted — the suite must pass whether or not OBS is open.
 * Exit code 0 = all checks green; 1 = at least one check failed.
 * ============================================================================
 */

static int sFailures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            sFailures++;                                               \
        }                                                              \
    } while (0)

// Verifies capture-tool registry rows, process/driver classification, and liveness probes.
// Checks CaptureTool discovery, metadata, and safe handling of missing inputs.
// Verifies capture-tool registry metadata and process/driver liveness probe behavior.
int main(int argc, const char **argv) {
    CaptureTool *tools = CaptureTool_shared();
    CHECK(tools != nullptr);
    const uint32_t total = CaptureTool_count(tools);
    printf("CaptureTool_count = %u\n", total);
    CHECK(total == 15);
    CHECK(CaptureTool_at(tools, 0) != nullptr);
    CHECK(CaptureTool_at(tools, total) == nullptr);

    // --- registry shape --------------------------------------------------------
    const CaptureSlot *obs = CaptureTool_get(tools, "obs");
    CHECK(obs != nullptr);
    CHECK(obs && strcmp(CaptureTool_getDisplayName(tools, obs), "OBS Studio") == 0);
    CHECK(obs && strcmp(CaptureTool_getProcKey(tools, obs), "obs") == 0);
    CHECK(obs && CaptureTool_getKind(tools, obs) == CAPTURE_KIND_SCREEN);

    const CaptureSlot *quicktime = CaptureTool_get(tools, "quicktime-player");
    CHECK(quicktime && strcmp(CaptureTool_getProcKey(tools, quicktime),
                              "QuickTime Player") == 0);

    const CaptureSlot *screenshot = CaptureTool_get(tools, "screenshot");
    CHECK(screenshot && strcmp(CaptureTool_getProcKey(tools, screenshot),
                               "screencaptureui") == 0);

    const CaptureSlot *voice = CaptureTool_get(tools, "voice-memos");
    CHECK(voice && CaptureTool_getKind(tools, voice) == CAPTURE_KIND_AUDIO);

    const CaptureSlot *zoom = CaptureTool_get(tools, "zoom");
    CHECK(zoom && CaptureTool_getKind(tools, zoom) == CAPTURE_KIND_STREAM);

    CHECK(CaptureTool_get(tools, "definitely-not-a-capture-tool") == nullptr);

    // --- driver-vs-process contract -------------------------------------------
    const CaptureSlot *blackhole = CaptureTool_get(tools, "blackhole");
    CHECK(blackhole && CaptureTool_getProcKey(tools, blackhole) == nullptr);
    CHECK(blackhole && CaptureTool_getDriverDir(tools, blackhole) != nullptr);
    const CaptureSlot *soundflower = CaptureTool_get(tools, "soundflower");
    CHECK(soundflower && CaptureTool_getProcKey(tools, soundflower) == nullptr);
    CHECK(soundflower && CaptureTool_getDriverDir(tools, soundflower) != nullptr);

    for (uint32_t i = 0; i < total; i++) {
        const CaptureSlot *row = CaptureTool_at(tools, i);
        CHECK(row != nullptr);
        CHECK(row && CaptureTool_getSlug(tools, row) != nullptr);
        CHECK(row && CaptureTool_getDisplayName(tools, row) != nullptr);
        CHECK(row && CaptureTool_getKind(tools, row) >= CAPTURE_KIND_SCREEN);
        CHECK(row && CaptureTool_getKind(tools, row) <= CAPTURE_KIND_AUDIO);
        // exactly one probe path per row: process XOR driver
        if (row) {
            const char *proc = CaptureTool_getProcKey(tools, row);
            const char *dir = CaptureTool_getDriverDir(tools, row);
            CHECK((proc && !dir) || (!proc && dir));
        }
    }

    // --- live liveness (reported, never asserted for real tools) ---------------
    printf("isRunning(obs) = %s\n",
           CaptureTool_isRunning(tools, obs) ? "yes" : "no");
    printf("isRunning(quicktime) = %s\n",
           CaptureTool_isRunning(tools, quicktime) ? "yes" : "no");
    printf("countRunningAll = %u\n", CaptureTool_countRunningAll(tools));
    printf("countRunning(screen) = %u\n",
           CaptureTool_countRunning(tools, CAPTURE_KIND_SCREEN));
    // Contract truths: driver rows never "run"; counts never exceed rows.
    CHECK(!CaptureTool_isRunning(tools, blackhole));
    CHECK(CaptureTool_countRunningAll(tools) <= total);
    CHECK(CaptureTool_countRunning(tools, CAPTURE_KIND_SCREEN) <= total);
    // Self-truthy liveness via ProcessProbe on our own executable.
    const char *selfName = argc > 0 && argv[0] ? strrchr(argv[0], '/') : nullptr;
    selfName = selfName ? selfName + 1 : (argc > 0 ? argv[0] : nullptr);
    ProcessProbe *probe = ProcessProbe_shared();
    CHECK(selfName && ProcessProbe_isRunning(probe, selfName));
    CHECK(!ProcessProbe_isRunning(probe, "definitely-not-a-proc-xyzzy"));
    CHECK(!ProcessProbe_isDriverLoaded(probe,
                                       "/definitely/not/a/dir-xyzzy", "obs"));

    // --- null-safety (Rule 24) -------------------------------------------------
    CHECK(CaptureTool_count(nullptr) == 0);
    CHECK(CaptureTool_at(nullptr, 0) == nullptr);
    CHECK(CaptureTool_get(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_get(tools, nullptr) == nullptr);
    CHECK(CaptureTool_isRunning(nullptr, nullptr) == false);
    CHECK(CaptureTool_isInstalled(nullptr, nullptr) == false);
    CHECK(CaptureTool_countRunning(nullptr, CAPTURE_KIND_SCREEN) == 0);
    CHECK(CaptureTool_countRunningAll(nullptr) == 0);
    CHECK(CaptureTool_getSlug(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_getDisplayName(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_getProcKey(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_getDriverDir(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_getNote(nullptr, nullptr) == nullptr);
    CHECK(CaptureTool_getKind(nullptr, nullptr) == CAPTURE_KIND_SCREEN);

    if (sFailures == 0) {
        printf("capture_tool_test: ALL CHECKS PASSED (%u capture tools)\n", total);
        return 0;
    }
    printf("capture_tool_test: %d FAILURES\n", sFailures);
    return 1;
}
