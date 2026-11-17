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
 *     _Atomic i32 state;         // 0 = STOPPED, 1 = RUNNING
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
    _Atomic i32 state;        // 0 = STOPPED, 1 = RUNNING
    u64 typeId;          // block header type (TYPE_THREAD_*)
    pthread_t platform;
    bool platformStarted;
    RingBuffer queue;         // any task slots
    Thread_Job job;
    bool tickWhenIdle;
    bool core;
} Thread;

;;GETTER
static inline bool Thread_core(const Thread *s) { return (*s).core; }
;;GETTER
static inline bool *Thread_coreMut(Thread *s) { return &(*s).core; }
;;SETTER
static inline fn Thread_setCore(Thread *s, bool v) { (*s).core = v; }
;;GETTER
static inline Thread_Job Thread_job(const Thread *s) { return (*s).job; }
;;GETTER
static inline Thread_Job *Thread_jobMut(Thread *s) { return &(*s).job; }
;;SETTER
static inline fn Thread_setJob(Thread *s, Thread_Job v) { (*s).job = v; }
;;GETTER
static inline pthread_t Thread_platform(const Thread *s) { return (*s).platform; }
;;GETTER
static inline pthread_t *Thread_platformMut(Thread *s) { return &(*s).platform; }
;;SETTER
static inline fn Thread_setPlatform(Thread *s, pthread_t v) { (*s).platform = v; }
;;GETTER
static inline bool Thread_platformStarted(const Thread *s) { return (*s).platformStarted; }
;;GETTER
static inline bool *Thread_platformStartedMut(Thread *s) { return &(*s).platformStarted; }
;;SETTER
static inline fn Thread_setPlatformStarted(Thread *s, bool v) { (*s).platformStarted = v; }
;;GETTER
static inline RingBuffer Thread_queue(const Thread *s) { return (*s).queue; }
;;GETTER
static inline RingBuffer *Thread_queueMut(Thread *s) { return &(*s).queue; }
;;SETTER
static inline fn Thread_setQueue(Thread *s, RingBuffer v) { (*s).queue = v; }
;;GETTER
static inline _Atomic i32 Thread_state(const Thread *s) { return (*s).state; }
;;GETTER
static inline _Atomic i32 *Thread_stateMut(Thread *s) { return &(*s).state; }
;;SETTER
static inline fn Thread_setState(Thread *s, _Atomic i32 v) { (*s).state = v; }
;;GETTER
static inline bool Thread_tickWhenIdle(const Thread *s) { return (*s).tickWhenIdle; }
;;GETTER
static inline bool *Thread_tickWhenIdleMut(Thread *s) { return &(*s).tickWhenIdle; }
;;SETTER
static inline fn Thread_setTickWhenIdle(Thread *s, bool v) { (*s).tickWhenIdle = v; }
;;GETTER
static inline u64 Thread_typeId(const Thread *s) { return (*s).typeId; }
;;GETTER
static inline u64 *Thread_typeIdMut(Thread *s) { return &(*s).typeId; }
;;SETTER
static inline fn Thread_setTypeId(Thread *s, u64 v) { (*s).typeId = v; }


#define THREAD_TASK_SIZE sizeof(any)

// Central registry: handle address => 1. Legacy used Map.put(workerPtr, 1L).
static Map *s_workers = nullptr;

static any platform_main(any arg);

;;GETTER
static u8 *RingBuffer_slots(const RingBuffer *ring) {
    return (*ring).slots;
}

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
    atomic_init(Thread_stateMut(t), 0);
    Thread_setTypeId(t, typeId);
    Thread_setPlatformStarted(t, false);
    Thread_setJob(t, job);
    Thread_setTickWhenIdle(t, tickWhenIdle);
    Thread_setCore(t, core);
    Thread_setQueue(t, RingBuffer(THREAD_TASK_SIZE, queueCapacity));
    if (RingBuffer_slots(Thread_queue(t)) == nullptr) {
        Memory_free(t);
        return nullptr;
    }
    Map_put(workers(), (u64)(pointer)t, 1);
    return t;
}

bool Thread_run(Thread *self) {
    if (!self)
        return false;
    i32 expected = 0;
    if (!atomic_compare_exchange_strong(Thread_stateMut(self), &expected, 1))
        return true; // already running
    if (Thread_platformStarted(self)) {
        // Re-running a joined handle: the old pthread_t is spent; start fresh.
        Thread_setPlatformStarted(self, false);
    }
    if (pthread_create(Thread_platformMut(self), nullptr, platform_main, self) != 0) {
        atomic_store(Thread_stateMut(self), 0);
        return false;
    }
    Thread_setPlatformStarted(self, true);
    return true;
}

bool Thread_submit(Thread *self, any task) {
    if (!self || !task)
        return false;
    if (atomic_load(Thread_stateMut(self)) != 1)
        return false;
    return RingBuffer_push(Thread_queueMut(self), &task);
}

static fn joinPlatform(Thread *self) {
    if (Thread_platformStarted(self)) {
        pthread_join(Thread_platform(self), nullptr);
        Thread_setPlatformStarted(self, false);
    }
}

fn Thread_stop(Thread *self) {
    if (!self || Thread_core(self))
        return;
    atomic_store(Thread_stateMut(self), 0);
    joinPlatform(self);
}

// force=true is the system-shutdown path: cores die too (legacy freeAllSystem).
static fn freeInternal(Thread *self, bool force) {
    if (!self)
        return;
    if (Thread_core(self) && !force)
        return;
    atomic_store(Thread_stateMut(self), 0);
    joinPlatform(self);
    RingBuffer_shutdown(Thread_queueMut(self));
    Map_remove(workers(), (u64)(pointer)self);
    Memory_free(self);
}

fn Thread_free(Thread *self) {
    freeInternal(self, false);
}

bool Thread_isRunning(Thread *self) {
    return self && atomic_load(Thread_stateMut(self)) == 1;
}

RingBuffer *Thread_queue(Thread *self) {
    return self ? Thread_queueMut(self) : nullptr;
}

u64 Thread_purpose(Thread *self) {
    return self ? Thread_typeId(self) : 0;
}

static any platform_main(any arg) {
    Thread *self = (Thread*) arg;
    while (atomic_load(Thread_stateMut(self)) == 1) {
        any task = nullptr;
        if (RingBuffer_pop(Thread_queueMut(self), &task)) {
            (*Thread_job(self))(self, task);
        } else if (Thread_tickWhenIdle(self)) {
            (*Thread_job(self))(self, nullptr);
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
    (discard)system;
    Thread_run(self);
}

static fn visitStop(Thread *self, bool system) {
    (discard)system;
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
