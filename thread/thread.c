#include "vex.h"
#include "thread/thread.h"

#include <pthread.h>
#include <time.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "struct/array.h"
#include "struct/map.h"

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Thread
 * ============================================================================
 * The worker-thread engine (Legacy: the six *Thread.java classes): a
 * supervised pthread wrapper with an atomic state flag, a RingBuffer task
 * queue, and an optional idle-tick job. Thread(...) allocates from the memory
 * arena and registers the handle in a central Map; Thread_run spawns the
 * platform thread, Thread_stop flips state and joins, Thread_free tears down
 * queue, registry entry, and block. Core handles are immune to stop/free
 * unless the system-shutdown path forces them. Lives at R2 as a leaf
 * threading behavior.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Thread (thread/thread.c)
 * ============================================================================
 * the worker-thread engine (Legacy: the six *Thread.java
 *
 * STRUCT FIELDS (local to this file):
 * ----------------------------------------------------------------------------
 *   Thread {
 *     _Atomic int state;         // 0 = STOPPED, 1 = RUNNING
 *     u64 typeId;           // block-header type (TYPE_THREAD_*)
 *     pthread_t platform;        // native platform thread handle
 *     bool platformStarted;      // platform thread spawned flag
 *     RingBuffer queue;          // any task slot ring
 *     Thread_Job job;            // per-pass task body callback
 *     bool tickWhenIdle;         // run job with nullptr task when idle
 *     bool core;                 // core handle: immune to stop/free
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Thread(typeId, job, queueCapacity, tickWhenIdle, core)
 *
 * Core Functions:
 *   - Thread_run(self)
 *   - Thread_submit(self, task)
 *   - Thread_stop(self)
 *   - Thread_free(self)
 *   - Thread_queue(self)
 *   - Thread_purpose(self)
 *   - Thread_runAll()
 *   - Thread_stopAll()
 *   - Thread_freeAll()
 *   - Thread_freeAllSystem()
 *   - Thread_count()
 *
 * Getters:
 *   - Thread_isRunning(self)
 * ============================================================================
 */


// thread/thread.c — the worker engine (Legacy: thread/*Thread.java).

typedef struct Thread {
    _Atomic int state;        // 0 = STOPPED, 1 = RUNNING
    u64 typeId;          // block header type (TYPE_THREAD_*)
    pthread_t platform;
    bool platformStarted;
    RingBuffer queue;         // any task slots
    Thread_Job job;
    bool tickWhenIdle;
    bool core;
} Thread;

#define THREAD_TASK_SIZE sizeof(any)

// Central registry: handle address => 1. Legacy used Map.put(workerPtr, 1L).
static Map *s_workers = nullptr;

static any platform_main(any arg);

static Map *workers() {
    if (!s_workers)
        s_workers = Map(ID_LONG, ID_LONG, 16);
    return s_workers;
}

Thread *Thread_5(u64 typeId, Thread_Job job, bytes queueCapacity,
                 bool tickWhenIdle, bool core) {
    if (!job || queueCapacity == 0)
        return nullptr;
    Thread *t = Memory_alloc(typeId, sizeof(Thread));
    if (!t)
        return nullptr;
    atomic_init(&(*t).state, 0);
    (*t).typeId = typeId;
    (*t).platformStarted = false;
    (*t).job = job;
    (*t).tickWhenIdle = tickWhenIdle;
    (*t).core = core;
    (*t).queue = RingBuffer(THREAD_TASK_SIZE, queueCapacity);
    if ((*t).queue.slots == nullptr) {
        Memory_free(t);
        return nullptr;
    }
    Map_put(workers(), (u64)(pointer)t, 1);
    return t;
}

bool Thread_run(Thread *self) {
    if (!self)
        return false;
    int expected = 0;
    if (!atomic_compare_exchange_strong(&(*self).state, &expected, 1))
        return true; // already running
    if ((*self).platformStarted) {
        // Re-running a joined handle: the old pthread_t is spent; start fresh.
        (*self).platformStarted = false;
    }
    if (pthread_create(&(*self).platform, nullptr, platform_main, self) != 0) {
        atomic_store(&(*self).state, 0);
        return false;
    }
    (*self).platformStarted = true;
    return true;
}

bool Thread_submit(Thread *self, any task) {
    if (!self || !task)
        return false;
    if (atomic_load(&(*self).state) != 1)
        return false;
    return RingBuffer_push(&(*self).queue, &task);
}

static fn joinPlatform(Thread *self) {
    if ((*self).platformStarted) {
        pthread_join((*self).platform, nullptr);
        (*self).platformStarted = false;
    }
}

fn Thread_stop(Thread *self) {
    if (!self || (*self).core)
        return;
    atomic_store(&(*self).state, 0);
    joinPlatform(self);
}

// force=true is the system-shutdown path: cores die too (legacy freeAllSystem).
static fn freeInternal(Thread *self, bool force) {
    if (!self)
        return;
    if ((*self).core && !force)
        return;
    atomic_store(&(*self).state, 0);
    joinPlatform(self);
    RingBuffer_shutdown(&(*self).queue);
    Map_remove(workers(), (u64)(pointer)self);
    Memory_free(self);
}

fn Thread_free(Thread *self) {
    freeInternal(self, false);
}

bool Thread_isRunning(Thread *self) {
    return self && atomic_load(&(*self).state) == 1;
}

RingBuffer *Thread_queue(Thread *self) {
    return self ? &(*self).queue : nullptr;
}

u64 Thread_purpose(Thread *self) {
    return self ? (*self).typeId : 0;
}

static any platform_main(any arg) {
    Thread *self = (Thread*) arg;
    while (atomic_load(&(*self).state) == 1) {
        any task = nullptr;
        if (RingBuffer_pop(&(*self).queue, &task)) {
            (*(*self).job)(self, task);
        } else if ((*self).tickWhenIdle) {
            (*(*self).job)(self, nullptr);
        } else {
            struct timespec idle = { 0, 1000 * 1000 }; // legacy Thread.sleep(1)
            nanosleep(&idle, nullptr);
        }
    }
    return nullptr;
}

static fn walkPool(fn (*visit)(Thread *self, bool system), bool system) {
    Map *pool = workers();
    if (!pool)
        return;
    Array *keys = Map_keys(pool);
    if (!keys)
        return;
    bytes count = Array_length(keys);
    for (bytes i = 0; i < count; i++) {
        Thread *t = (Thread*) (pointer)Array_get(keys, i);
        visit(t, system);
    }
    Array_free(keys);
}

static fn visitRun(Thread *self, bool system) {
    (fn)system;
    Thread_run(self);
}

static fn visitStop(Thread *self, bool system) {
    (fn)system;
    Thread_stop(self);
}

static fn visitFree(Thread *self, bool system) {
    freeInternal(self, system);
}

fn Thread_runAll() {
    walkPool(visitRun, false);
}

fn Thread_stopAll() {
    walkPool(visitStop, false);
}

fn Thread_freeAll() {
    walkPool(visitFree, false);
}

fn Thread_freeAllSystem() {
    walkPool(visitFree, true);
}

bytes Thread_count() {
    return s_workers ? Map_size(s_workers) : 0;
}

const char *ThreadRole_name(ThreadRole role) {
    switch (role) {
        case THREAD_ROLE_MAIN:       return "_main";
        case THREAD_ROLE_ENGINE:     return "engine";
        case THREAD_ROLE_DRAW:       return "draw";
        case THREAD_ROLE_PRESENT:    return "present";
        case THREAD_ROLE_NETWORKING: return "networking";
        case THREAD_ROLE_SCRIPTING:  return "scripting";
        case THREAD_ROLE_CONSOLE:    return "console";
        case THREAD_ROLE_UI:         return "ui";
        case THREAD_ROLE_USER:       return "user";
        default:                     return "none";
    }
}
