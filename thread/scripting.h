#ifndef THREAD_SCRIPTING_H
#define THREAD_SCRIPTING_H

#include "thread/thread.h"

// thread/scripting.h — the scripting worker (Legacy: ScriptingThread.java).
//
// Drains script/task handles off the _main thread. Pure queue consumer:
// no idle ticking.

Thread *ScriptingThread_invoke();
bool ScriptingThread_submit(Thread *w, any task);
fn ScriptingThread_stop(Thread *w);
fn ScriptingThread_free(Thread *w);

#endif
