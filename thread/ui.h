#ifndef THREAD_UI_H
#define THREAD_UI_H

#include "thread/thread.h"

// thread/ui.h — the UI worker (Legacy: UIThread.java).
//
// Owns component lifecycle ticks (caret blinks, scroll inertia) and a packet
// queue. tickWhenIdle is on so components animate even with no packets.

Thread *UIThread_invoke();
bool UIThread_submit(Thread *w, any packet);
fn UIThread_stop(Thread *w);
fn UIThread_free(Thread *w);

#endif
