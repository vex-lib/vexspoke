#include "vex.h"
#include "thread/scripting.h"
#include "thread/compute.h"

#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Scripting
 * ============================================================================
 * The scripting worker (Legacy: ScriptingThread.java). Script executor port:
 * tasks are dequeued and dropped until the scripting surface lands, while the
 * queue and lifecycle are fully live. ScriptingThread_invoke creates a
 * scripting-role Thread whose job currently discards tasks; submit/stop/free
 * delegate straight to the Thread class.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Scripting (thread/scripting.c)
 * ============================================================================
 * the scripting worker (Legacy: ScriptingThread.java).
 *
 * STRUCT FIELDS: none — procedural (operates on Thread pool (scripting role workers))
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - ScriptingThread_invoke()
 *   - ScriptingThread_submit(w, task)
 *   - ScriptingThread_stop(w)
 *   - ScriptingThread_free(w)
 * ============================================================================
 */


// thread/scripting.c — scripting worker (Legacy: ScriptingThread.java).

;;INCOMPLETE // Script executor port: tasks are dequeued and dropped until the
;;INCOMPLETE // scripting surface lands. Queue and lifecycle are fully live.

static fn scripting_job(Thread *self, any task) {
    (fn)self;
    if (ComputeJob_run(task)) return;   // a generic job handed to this role
    (fn)task; // TODO(lang): execute(task) once the script surface lands.
}

Thread *ScriptingThread_invoke() {
    return Thread(TYPE_THREAD_SCRIPTING_SINGLETON, scripting_job,
                      1024, false, false);
}

bool ScriptingThread_submit(Thread *w, any task) {
    return Thread_submit(w, task);
}

fn ScriptingThread_stop(Thread *w) {
    Thread_stop(w);
}

fn ScriptingThread_free(Thread *w) {
    Thread_free(w);
}
