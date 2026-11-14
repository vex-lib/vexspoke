#include "vex.h"
#include "system/browser.h"


#if defined(VEX_PLATFORM_WINDOWS)
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Browser
 * ============================================================================
 * The system browser launcher: navigates to external URLs using the operating
 * system's registered default web browser. Pure platform invocation via
 * posix_spawnp (open on macOS, xdg-open on Linux) or ShellExecute on Windows —
 * never invokes a shell (preventing command injection), never blocks the UI
 * event loop, and performs zero heap allocation.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Browser (system/browser)
 * ============================================================================
 * STRUCT FIELDS (Mirroring system/browser.h):
 * ----------------------------------------------------------------------------
 *   u32 reserved;  // singleton marker; no mutable state
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructor:
 *   - Browser_shared()
 * Core Functions:
 *   - Browser_open(url)
 *   - Browser_openUrl(self, url)
 * ============================================================================
 */
;;INTENTION("default browser invocation: posix_spawnp with direct argument vector; never system() or sh -c")

static Browser gSharedBrowser = { 0u };

Browser *Browser_shared() {
    return &gSharedBrowser;
}

bool Browser_openUrl(const Browser *self, const char *url) {
    if (self == nullptr || url == nullptr || *url == '\0')
        return false;

#if defined(VEX_PLATFORM_WINDOWS)
    HINSTANCE instance = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return (pointer) instance > 32u;
#else
    const char *program =
#if defined(VEX_PLATFORM_APPLE)
        "open";
#else
        "xdg-open";
#endif

    char *const arguments[] = {
        (char*) program,
        (char*) url,
        nullptr
    };

    pid_t childPid = 0;
    int spawnStatus = posix_spawnp(&childPid, program, nullptr, nullptr, arguments, environ);
    if (spawnStatus != 0)
        return false;

    int waitStatus = 0;
    if (waitpid(childPid, &waitStatus, 0) < 0)
        return false;

    return WIFEXITED(waitStatus) && WEXITSTATUS(waitStatus) == 0;
#endif
}

bool Browser_open(const char *url) {
    return Browser_openUrl(Browser_shared(), url);
}
