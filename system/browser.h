#ifndef SYSTEM_BROWSER_H
#define SYSTEM_BROWSER_H

#include "vex.h"

// system/browser.h — Default web browser launcher.
//
// Opens URLs in the platform's default web browser:
// - macOS: spawns `open <url>` via posix_spawnp.
// - Linux / POSIX: spawns `xdg-open <url>` via posix_spawnp.
// - Windows: spawns browser launcher.
// Pure POSIX / OS API, zero shell invocation, zero allocation.

typedef struct Browser {
    u32 reserved;
} Browser;

// --- Constructor ---
// Returns the shared browser launcher handle (static, zero-init, never NULL).
Browser *Browser_shared();

// --- Core functions ---
// Launch the default browser to navigate to the specified URL.
// Returns true on successful process spawn; false on NULL, empty or spawn failure.
bool Browser_open(const char *url);
bool Browser_openUrl(const Browser *self, const char *url);

#endif
