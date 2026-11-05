// vexspoke/test/system/process_probe_test.c — owner test for
// system/process_probe.
//
// Proves the process/driver liveness probe:
//   - the shared handle is a stable singleton;
//   - isRunning matches our own executable by basename, case-insensitively,
//     and rejects an absent name, empty, and nullptr;
//   - isDriverLoaded scans a directory for a case-insensitive prefix match,
//     and rejects a missing directory, an absent prefix, empty, and nullptr.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "system/process_probe.h"
#include "io/file.h"

static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_failures++;                                                  \
        }                                                                  \
    } while (0)

// Tests singleton process/driver probes against the current process and a scratch directory.
// Exercises process-probe matching and verifies the reported detection result.
// Tests process-name lookup and case-insensitive driver-directory prefix scanning.
int main(void) {
    ProcessProbe *p = ProcessProbe_shared();
    CHECK(p != nullptr);
    CHECK(ProcessProbe_shared() == p);                 // singleton

    // Our own executable basename is "process_probe_test".
    CHECK(ProcessProbe_isRunning(p, "process_probe_test"));
    CHECK(ProcessProbe_isRunning(p, "PROCESS_PROBE_TEST"));   // case-insensitive
    CHECK(!ProcessProbe_isRunning(p, "definitely-not-a-real-process-9f3a"));
    CHECK(!ProcessProbe_isRunning(p, ""));
    CHECK(!ProcessProbe_isRunning(p, nullptr));
    CHECK(!ProcessProbe_isRunning(nullptr, "launchd"));

    // Driver-directory prefix scan in a scratch directory.
    const char *base = getenv("TMPDIR");
    if (!base || !*base)
        base = "/tmp";
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/vexspoke_probe_%d", base, getpid());
    File_delete(dir);
    CHECK(File_mkdirs(dir));
    char driver[300];
    snprintf(driver, sizeof(driver), "%s/BlackHole2ch.driver", dir);
    File *f = File_open(driver, FILE_MODE_WRITE | FILE_MODE_CREATE | FILE_MODE_TRUNCATE);
    CHECK(f != nullptr);
    if (f)
        File_close(f);

    CHECK(ProcessProbe_isDriverLoaded(p, dir, "blackhole"));   // case-insensitive
    CHECK(ProcessProbe_isDriverLoaded(p, dir, "BlackHole"));
    CHECK(ProcessProbe_isDriverLoaded(p, dir, "Black"));       // prefix
    CHECK(!ProcessProbe_isDriverLoaded(p, dir, "soundflower"));
    CHECK(!ProcessProbe_isDriverLoaded(p, dir, ""));
    CHECK(!ProcessProbe_isDriverLoaded(p, "/no/such/dir", "x"));
    CHECK(!ProcessProbe_isDriverLoaded(p, nullptr, "x"));
    CHECK(!ProcessProbe_isDriverLoaded(p, dir, nullptr));
    CHECK(!ProcessProbe_isDriverLoaded(nullptr, dir, "x"));

    File_delete(driver);
    File_delete(dir);

    if (g_failures == 0) {
        printf("process_probe_test: all assertions held\n");
        return 0;
    }
    printf("process_probe_test: %d FAILURES\n", g_failures);
    return 1;
}
