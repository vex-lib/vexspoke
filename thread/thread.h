#ifndef THREAD_THREAD_H
#define THREAD_THREAD_H

#include "vex.h"

#include "struct/ring.h"

// thread/thread.h — the worker-thread engine (Legacy: the six *Thread.java
// managers that each re-implemented the same 200 lines).
//
// A thread handle is a self-describing block: [typeId][len][state|queue|job…].
// The engine owns the lifecycle — spawn, join, submit, stop, free, registry,
// core protection — while each purpose class (networking/event/draw/scripting/
// ui) supplies only its per-pass job:
//
//   Thread_Job: called once per popped task (task != nullptr) or once per idle
//   pass when tickWhenIdle is set (task == nullptr).
//
// Loop semantics mirror legacy: drain queue → run job; sleep 1ms when idle
// unless tickWhenIdle; exit when state leaves RUNNING.

// ThreadRole — the host's thread roles. Policy, not mechanism: re's thread
// table stores the value as an opaque u32 tag (re/thread/thread.h).

// Role tags. 0 = untagged.
typedef enum ThreadRole {
    THREAD_ROLE_NONE       = 0,
    THREAD_ROLE_MAIN       = 1, // Thread 0: owns AppKit / the OS event pump
    THREAD_ROLE_ENGINE     = 2, // the fixed-timestep engine Loop
    THREAD_ROLE_DRAW       = 3, // render loop owner
    THREAD_ROLE_PRESENT    = 4, // swapchain blit cadence owner
    THREAD_ROLE_NETWORKING = 5,
    THREAD_ROLE_SCRIPTING  = 6,
    THREAD_ROLE_CONSOLE    = 7,
    THREAD_ROLE_UI         = 8, // darling layout/draw when it lands
    THREAD_ROLE_USER       = 9, // user-generated threads
} ThreadRole;

// Stable symbolic name, e.g. "draw". Unknown and untagged roles read "none".
const char *ThreadRole_name(ThreadRole role);

typedef struct Thread Thread;

// Per-pass body. Runs on the worker platform thread.
typedef void (*Thread_Job)(Thread *self, any task);

// Create a stopped handle. typeId stamps the block header (one of the
// ID_THREAD_* classes); queueCapacity holds any tasks. Core handles reject
// stop/free like legacy ROLE_CORE. nullptr on OOM/bad args.
Thread *Thread_5(u64 typeId, Thread_Job job, bytes queueCapacity,
                 bool tickWhenIdle, bool core);
#define Thread(...) CONSTRUCTOR_DISPATCH(Thread, __VA_ARGS__)

// Spawn the platform thread. True on success or if already running.
bool Thread_run(Thread *self);

// Enqueue a task handle. False when stopped or the ring is full.
bool Thread_submit(Thread *self, any task);

// Ask the loop to exit and join. No-op when already stopped. Refused on cores.
void Thread_stop(Thread *self);

// Stop, tear down the ring, unregister, release the block. Cores are immune
// until shutdown calls Thread_freeAllSystem().
void Thread_free(Thread *self);

bool Thread_isRunning(Thread *self);
RingBuffer *Thread_queue(Thread *self);
u64 Thread_purpose(Thread *self);

// Pool-wide walkers over the central registry (legacy runAll/stopAll/freeAll).
void Thread_runAll(void);
void Thread_stopAll(void);
void Thread_freeAll(void);

// Shutdown-only: frees even core handles. Nothing survives this call.
void Thread_freeAllSystem(void);

bytes Thread_count(void);

#endif
