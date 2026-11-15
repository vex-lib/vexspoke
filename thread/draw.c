#include "vex.h"
#include "thread/draw.h"
#include "thread/compute.h"

#include "oop/vexspoke_type.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Draw
 * ============================================================================
 * The rendering worker pool: a fixed 8-slot RoleSlot table, each slot
 * binding a DRAW_ROLE_* tag to a supervised Thread worker. invokeRole lazily
 * spawns the first free slot for a role (core roles get a core-affine
 * thread); submitTo routes a task to the role's worker; stopAll/freeAll tear
 * the pool down in order. The renderer backend is still INCOMPLETE — role
 * dispatch receives tasks but has no renderer to record into yet; queue
 * plumbing and role routing are fully live.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Draw (thread/draw.c)
 * ============================================================================
 * the rendering worker pool (Legacy: DrawThread.java).
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   RoleSlot {
 *     i32 role;                  // DRAW_ROLE_* tag served by this slot
 *     Thread *worker;            // backing worker thread handle
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - DrawThread_invokeRole(role)
 *   - DrawThread_submitTo(role, task)
 *   - DrawThread_stopAll()
 *   - DrawThread_freeAll()
 * ============================================================================
 */


// thread/draw.c — rendering worker pool (Legacy: DrawThread.java).

;;INCOMPLETE // Render backend port: role dispatch receives tasks but has no
;;INCOMPLETE // renderer to record into (legacy UserRole.record / gradient
;;INCOMPLETE // scene). Queue plumbing and role routing are fully live.

#define DRAW_ROLE_SLOTS 8

typedef struct RoleSlot {
    i32 role;
    Thread *worker;
} RoleSlot;

static RoleSlot s_roles[DRAW_ROLE_SLOTS];

static Thread *roleWorker(i32 role) {
    for (i32 i = 0; i < DRAW_ROLE_SLOTS; i++) {
        if (s_roles[i].worker && s_roles[i].role == role)
            return s_roles[i].worker;
    }
    return nullptr;
}

static fn draw_job(Thread *self, any task) {
    (discard)self;
    if (ComputeJob_run(task)) return;   // a generic job handed to this role
    if (!task)
        return;
    // TODO(draw): route by slot role into the renderer once it lands.
}

Thread *DrawThread_invokeRole(i32 role) {
    for (i32 i = 0; i < DRAW_ROLE_SLOTS; i++) {
        if (!s_roles[i].worker) {
            bool core = role == DRAW_ROLE_CORE;
            Thread *w = Thread(TYPE_THREAD_DRAW_SINGLETON, draw_job,
                                   1024, false, core);
            if (!w)
                return nullptr;
            s_roles[i].role = role;
            s_roles[i].worker = w;
            return w;
        }
    }
    return nullptr;
}

bool DrawThread_submitTo(i32 role, any task) {
    Thread *w = roleWorker(role);
    return w && Thread_submit(w, task);
}

fn DrawThread_stopAll() {
    for (i32 i = 0; i < DRAW_ROLE_SLOTS; i++)
        Thread_stop(s_roles[i].worker);
}

fn DrawThread_freeAll() {
    for (i32 i = 0; i < DRAW_ROLE_SLOTS; i++) {
        Thread_free(s_roles[i].worker);
        s_roles[i].worker = nullptr;
        s_roles[i].role = 0;
    }
}
