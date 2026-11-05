#include <pthread.h>
#include <stdio.h>

#include "annotation/annotation.h"
#include "reactive/dispatch.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReactiveTest (vexspoke/test/reactive_test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Proves the per-variable event emitter: multiple observers per event, set never
 * fires (it stores + marks dirty), drain coalesces the batch into one onSet and
 * one onChanged on a real move, add/remove, a mid-fire removal is safe, and a
 * CROSS-THREAD writer is safe (the owner drains).
 *
 * STRUCT FIELDS: none — procedural test harness.
 * ============================================================================
 */

#define CHECK(cond)                                                          \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                      \
        }                                                                    \
    } while(0)

static int g_setA, g_setB, g_changedA, g_changedB;
static Reactive *g_self;
static ReactiveChangedFn g_changedFnA;

// Counts delivery to the first onSet observer.
static void onSetA(Reactive *r, uintptr_t v, void *ud) { (void) r; (void) v; (void) ud; g_setA++; }
// Counts delivery to the second onSet observer.
static void onSetB(Reactive *r, uintptr_t v, void *ud) { (void) r; (void) v; (void) ud; g_setB++; }
// Counts changed notifications and removes observer A during dispatch to test safe mutation.
static void onChangedA(Reactive *r, uintptr_t o, uintptr_t n, void *ud) {
    (void) o; (void) n; (void) ud;
    g_changedA++;
    // mid-fire removal: A removes itself; the fire must stay safe.
    if (r == g_self)
        Reactive_removeOnChanged(r, g_changedFnA, nullptr);
}
// Counts delivery to the second onChanged observer.
static void onChangedB(Reactive *r, uintptr_t o, uintptr_t n, void *ud) { (void) r; (void) o; (void) n; (void) ud; g_changedB++; }

// A writer thread: hammer the reactive from a foreign thread. Must never fire an
// observer there — it only stores + marks dirty.
typedef struct SetterArg { Reactive *r; uint64_t n; } SetterArg;
// Writes a sequence from a worker thread; observer delivery is left to the owner drain.
static void *setterLoop(void *arg) {
    SetterArg *a = (SetterArg*) arg;
    for (uint64_t i = 1; i <= (*a).n; i++)
        Reactive_set((*a).r, i);
    return nullptr;
}

// Proves observer fan-out/removal, owner-affine coalesced drain, thread-safe writes, and null handling.
int main(void) {
    int failures = 0;

    Reactive *r = Reactive_1(0);
    g_self = r;
    g_changedFnA = onChangedA;
    CHECK(r != nullptr);

    // 1. Multiple observers per event.
    CHECK(Reactive_addOnSet(r, onSetA, nullptr));
    CHECK(Reactive_addOnSet(r, onSetB, nullptr));
    CHECK(Reactive_addOnChanged(r, onChangedA, nullptr));
    CHECK(Reactive_addOnChanged(r, onChangedB, nullptr));
    CHECK(Reactive_observerCount(r) == 4);

    // 2. set stores + marks dirty but NEVER fires; drain fires onSet (both) +
    //    onChanged (both) on a real move.
    Reactive_set(r, 5);
    CHECK(g_setA == 0 && g_setB == 0 && g_changedA == 0 && g_changedB == 0);
    CHECK(Reactive_isDirty(r) == true);
    CHECK(Reactive_get(r) == 5);
    CHECK(Reactive_drain(r) == true);
    CHECK(g_setA == 1 && g_setB == 1);
    CHECK(g_changedA == 1 && g_changedB == 1);
    CHECK(Reactive_isDirty(r) == false);

    // 3. A same-value write fires onSet (a batch happened) but NOT onChanged.
    Reactive_set(r, 5);
    Reactive_drain(r);
    CHECK(g_setA == 2 && g_setB == 2);
    CHECK(g_changedA == 1 && g_changedB == 1);

    // 4. onChangedA removed itself mid-fire (step 2); now only B fires.
    Reactive_set(r, 7);
    Reactive_drain(r);
    CHECK(g_changedA == 1 && g_changedB == 2);

    // 5. remove B; onChanged is silent.
    CHECK(Reactive_removeOnChanged(r, onChangedB, nullptr));
    Reactive_set(r, 9);
    Reactive_drain(r);
    CHECK(g_changedA == 1 && g_changedB == 2);

    // 6. free is just free — the old teardown channel is gone.
    Reactive_free(r);

    // 7. Cross-thread: a writer thread moves the value; the owner drains.
    g_self = nullptr;                       // keep the mid-fire path inert
    g_setA = g_setB = g_changedA = g_changedB = 0;
    Reactive *t = Reactive_1(0);
    CHECK(Reactive_addOnSet(t, onSetA, nullptr));
    CHECK(Reactive_addOnChanged(t, onChangedB, nullptr));
    SetterArg arg = { t, 1000 };
    pthread_t th;
    CHECK(pthread_create(&th, nullptr, setterLoop, &arg) == 0);
    CHECK(pthread_join(th, nullptr) == 0);
    CHECK(Reactive_get(t) == 1000);          // atomic load sees the last write
    CHECK(Reactive_isDirty(t) == true);      // a batch awaits the owner
    CHECK(g_setA == 0 && g_changedB == 0);   // NO observer fired on the writer
    CHECK(Reactive_drain(t) == true);        // the owner fires the batch
    CHECK(g_setA == 1 && g_changedB == 1);   // coalesced: one batch, not 1000
    CHECK(Reactive_isDirty(t) == false);
    CHECK(Reactive_drain(t) == false);       // nothing new to drain
    Reactive_free(t);

    // 8. Cold seams.
    CHECK(Reactive_addOnSet(nullptr, onSetA, nullptr) == false);
    CHECK(Reactive_addOnChanged(r, nullptr, nullptr) == false);
    CHECK(Reactive_removeOnChanged(nullptr, onChangedB, nullptr) == false);
    CHECK(Reactive_get(nullptr) == 0);
    CHECK(Reactive_observerCount(nullptr) == 0);
    CHECK(Reactive_isDirty(nullptr) == false);
    CHECK(Reactive_drain(nullptr) == false);
    Reactive_set(nullptr, 1);
    Reactive_free(nullptr);

    if (failures == 0)
        printf("PASS reactive_test: fan-out, atomic set/drain, mid-fire remove, cross-thread, teardown\n");
    else
        fprintf(stderr, "FAIL reactive_test: %d assertion(s) failed\n", failures);
    return failures == 0 ? 0 : 1;
}
