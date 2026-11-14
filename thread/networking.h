#ifndef THREAD_NETWORKING_H
#define THREAD_NETWORKING_H

#include "thread/thread.h"

// thread/networking.h — the networking worker (Legacy: NetworkingThread.java).
//
// Owns the HTTP/polling request queue: producers submit batch handles, the
// worker drains them off-thread. The core worker is created on first ask and
// protected from stop/free like legacy.

Thread *NetworkingThread_invoke();
Thread *NetworkingThread_core();      // shared singleton, created lazily
bool NetworkingThread_submit(Thread *w, any batch);
fn NetworkingThread_stop(Thread *w);    // no-op on the core
fn NetworkingThread_free(Thread *w);    // no-op on the core

#endif
