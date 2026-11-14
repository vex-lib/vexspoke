#ifndef CLI_CONSOLE_H
#define CLI_CONSOLE_H

#include "vex.h"

// Bare synchronous cold stdout output, not a managed terminal API. Control bytes
// are escaped, so each call produces exactly one line. No allocation or queue.
// init/drain/shutdown remain compatibility no-ops for System callers.
fn Console_init();
fn Console_log(const char *message);
// Consumes a live string block, even if stdout fails. Null reports rejection.
fn Console_logString(u8 *message);
fn Console_drain();
fn Console_shutdown();
#endif
