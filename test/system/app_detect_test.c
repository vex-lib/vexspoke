#include "util/annotation.h"
#include "system/app_detect.h"
#include "system/process_probe.h"

#include <stdio.h>
#include <string.h>

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: AppDetectTest (app_detect_test/test.c)
 * LEVEL: L2 — Behavior verification (headless; POSIX probes only)
 * ============================================================================
 * Executable proof of the app registry (moved from api-haven per user
 * direction — vexspoke R1 owns system probes): registry shape (count,
 * get, altProcs, getters), PATH resolution (which/isOnPath),
 * /Applications bundle scanning, live process liveness (isRunning via
 * ProcessProbe), and every null-safety guard.
 *
 * Environment-tolerant assertions: `ls`/`sh` are guaranteed on any POSIX
 * PATH; bundle checks assert absence for a nonsense name and report
 * known-tool install state without failing on machines where they are
 * absent. Process-liveness truthiness is proven on the test's own
 * executable basename via ProcessProbe.
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

// Checks the application registry, PATH/bundle probes, liveness, and null guards.
// Checks AppDetect command-line discovery against the supplied test arguments.
// Checks AppDetect registry access, host probes, and null-safe public entry points.
int main(int argc, const char **argv) {
    AppDetect *detect = AppDetect_shared();
    CHECK(detect != nullptr);
    printf("AppDetect_count = %u\n", AppDetect_count(detect));
    CHECK(AppDetect_count(detect) == 14);

    // --- registry ------------------------------------------------------------
    const AppSlot *opencode = AppDetect_get(detect, "opencode");
    CHECK(opencode != nullptr);
    CHECK(opencode && strcmp(AppDetect_getName(detect, opencode), "opencode") == 0);
    CHECK(opencode && AppDetect_getDisplayName(detect, opencode) != nullptr);
    CHECK(AppDetect_get(detect, "hermes") != nullptr);
    CHECK(AppDetect_get(detect, "nous") != nullptr);
    CHECK(AppDetect_get(detect, "codex") != nullptr);
    CHECK(AppDetect_get(detect, "claude") != nullptr);
    CHECK(AppDetect_get(detect, "t3") != nullptr);
    CHECK(AppDetect_get(detect, "grok") != nullptr);
    CHECK(AppDetect_get(detect, "gemini") != nullptr);
    CHECK(AppDetect_get(detect, "aider") != nullptr);
    CHECK(AppDetect_get(detect, "goose") != nullptr);
    CHECK(AppDetect_get(detect, "cline") != nullptr);
    CHECK(AppDetect_get(detect, "qwen-code") != nullptr);
    CHECK(AppDetect_get(detect, "continue") != nullptr);
    CHECK(AppDetect_get(detect, "definitely-not-a-tool-xyzzy") == nullptr);
    CHECK(AppDetect_at(detect, 0) != nullptr);
    CHECK(AppDetect_at(detect, AppDetect_count(detect)) == nullptr);

    // --- expanded rows --------------------------------------------------------
    const AppSlot *t3 = AppDetect_get(detect, "t3");
    CHECK(t3 && strcmp(AppDetect_getName(detect, t3), "t3") == 0);
    CHECK(t3 && strcmp(AppDetect_getAltProcs(detect, t3), "T3 Code") == 0);
    const AppSlot *cursor = AppDetect_get(detect, "cursor");
    CHECK(cursor && strcmp(AppDetect_getAltProcs(detect, cursor), "cursor-agent") == 0);
    const AppSlot *claude = AppDetect_get(detect, "claude");
    CHECK(claude && strcmp(AppDetect_getDisplayName(detect, claude), "Claude Code") == 0);

    // --- PATH resolution ------------------------------------------------------
    char path[4096];
    CHECK(AppDetect_which(detect, "ls", path, sizeof(path)));
    CHECK(strstr(path, "/ls") != nullptr);
    CHECK(AppDetect_isOnPath(detect, "sh"));
    CHECK(!AppDetect_isOnPath(detect, "definitely-not-a-bin-xyzzy"));
    CHECK(!AppDetect_which(detect, "definitely-not-a-bin-xyzzy", path, sizeof(path)));

    // --- app bundles ----------------------------------------------------------
    CHECK(!AppDetect_isAppBundle(detect, "definitely-not-an-app-xyzzy"));
    printf("isAppBundle(Safari) = %s\n",
           AppDetect_isAppBundle(detect, "Safari") ? "yes" : "no");
    printf("isInstalled(opencode) = %s\n",
           AppDetect_isInstalled(detect, opencode) ? "yes" : "no");
    printf("isInstalled(hermes) = %s\n",
           AppDetect_isInstalled(detect, AppDetect_get(detect, "hermes")) ? "yes" : "no");
    printf("isInstalled(t3) = %s\n",
           AppDetect_isInstalled(detect, t3) ? "yes" : "no");

    // --- live process liveness ------------------------------------------------
    // Truthy probe: our own executable is running (basename of argv[0]).
    const char *selfName = argc > 0 && argv[0] ? strrchr(argv[0], '/') : nullptr;
    selfName = selfName ? selfName + 1 : (argc > 0 ? argv[0] : nullptr);
    ProcessProbe *probe = ProcessProbe_shared();
    CHECK(selfName && ProcessProbe_isRunning(probe, selfName));
    // Reported, not asserted: machine-state dependent.
    printf("isRunning(opencode) = %s\n",
           AppDetect_isRunning(detect, opencode) ? "yes" : "no");
    printf("isRunning(t3) = %s\n",
           AppDetect_isRunning(detect, t3) ? "yes" : "no");

    // --- null-safety (Rule 24) ------------------------------------------------
    CHECK(AppDetect_count(nullptr) == 0);
    CHECK(AppDetect_get(nullptr, nullptr) == nullptr);
    CHECK(AppDetect_get(detect, nullptr) == nullptr);
    CHECK(AppDetect_at(nullptr, 0) == nullptr);
    CHECK(AppDetect_getName(nullptr, nullptr) == nullptr);
    CHECK(AppDetect_getAltProcs(nullptr, nullptr) == nullptr);
    CHECK(AppDetect_getNote(nullptr, nullptr) == nullptr);
    CHECK(AppDetect_isOnPath(nullptr, "ls") == false);
    CHECK(AppDetect_isAppBundle(nullptr, "Safari") == false);
    CHECK(AppDetect_isInstalled(nullptr, nullptr) == false);
    CHECK(AppDetect_isRunning(nullptr, nullptr) == false);
    CHECK(AppDetect_isRunning(detect, nullptr) == false);
    CHECK(!AppDetect_which(nullptr, "ls", path, sizeof(path)));
    CHECK(!AppDetect_which(detect, "ls", nullptr, 0));
    CHECK(ProcessProbe_isRunning(nullptr, nullptr) == false);
    CHECK(ProcessProbe_isDriverLoaded(nullptr, nullptr, nullptr) == false);

    if (sFailures == 0) {
        printf("app_detect_test: ALL CHECKS PASSED\n");
        return 0;
    }
    printf("app_detect_test: %d FAILURES\n", sFailures);
    return 1;
}
