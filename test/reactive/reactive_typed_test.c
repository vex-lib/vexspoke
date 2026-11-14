#include "vex.h"
#include <math.h>

#include "nio/mem.h"
#include "oop/vexspoke_type.h"
#include "reactive/reactive_primitive.h"
#include "reactive/reactive_object.h"
#include "reactive/reactive_probable.h"
#include "reactive/dispatch.h"

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: ReactiveTypedTest (reactive_typed_test/test.c)
 * LEVEL: L3 — Module Code (headless verification harness)
 * ============================================================================
 * Headless suite for the typed reactives (bool / int / d64 / string): the
 * embed-first contract, per-type set/get, bit-exact d64 round-trip, pointer
 * rebind as change, observer fan-out, batch coalescing, out-of-band raw writes,
 * and null-safety.
 *
 * STRUCT FIELDS: none — procedural test harness.
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - main()
 * ============================================================================
 */

static int g_fail = 0;
static int g_checks = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        g_checks++;                                                       \
        if (!(cond)) {                                                    \
            g_fail++;                                                     \
            printf("  FAIL line %d: %s\n", __LINE__, #cond);              \
        }                                                                 \
    } while (0)

static int g_changed = 0;
static int g_sets = 0;
static i32 g_old = 0;
static i32 g_new = 0;
static Vec3 *g_objectValue = nullptr;
static i32 g_probableValue = 0;

static fn onObjectChanged(Vec3 *value) { g_objectValue = value; }
static fn onProbableChanged(i32 value) { g_probableValue = value; }

// Records typed integer change notifications and their old/new payloads.
static fn onChangedInt(Reactive *r, pointer o, pointer n, any ud) {
    (fn) r;
    (fn) ud;
    g_changed++;
    g_old = (i32) (u32) o;
    g_new = (i32) (u32) n;
}

// Counts typed integer onSet notifications.
static fn onSetInt(Reactive *r, pointer v, any ud) {
    (fn) r;
    (fn) v;
    (fn) ud;
    g_sets++;
}

// Verifies a typed reactive begins at the same address as its raw Reactive base.
static fn testEmbedFirst() {
    printf("[1] embed-first: a typed reactive* is a Reactive*\n");
    ReactiveInt *h = ReactiveInt(7);
    CHECK(h != nullptr);
    if (h) {
        CHECK((pointer) h == (pointer) &(*h).base);
        CHECK(Reactive_isActive(&(*h).base) == true);
        ReactiveInt_free(h);
    }
}

// Checks integer access, notifications/coalescing, raw writes, and negative values.
static fn testInt() {
    printf("[2] int: set/get, exact old/new, coalescing, out-of-band write\n");
    ReactiveInt *h = ReactiveInt(10);
    CHECK(h != nullptr);
    if (!h)
        return;
    CHECK(ReactiveInt_get(h) == 10);

    Reactive_addOnChanged(&(*h).base, onChangedInt, nullptr);
    Reactive_addOnSet(&(*h).base, onSetInt, nullptr);

    ReactiveInt_set(h, 20);
    CHECK(Reactive_drain(&(*h).base) == true);
    CHECK(g_changed == 1 && g_old == 10 && g_new == 20);
    CHECK(g_sets == 1);
    CHECK(ReactiveInt_get(h) == 20);

    // Coalesce: three writes -> ONE set, ONE change (last wins).
    ReactiveInt_set(h, 21);
    ReactiveInt_set(h, 22);
    ReactiveInt_set(h, 23);
    CHECK(Reactive_drain(&(*h).base) == true);
    CHECK(g_changed == 2 && g_old == 20 && g_new == 23);
    CHECK(g_sets == 2);

    // No write -> no fire.
    CHECK(Reactive_drain(&(*h).base) == false);

    // Out-of-band: a raw engine write to the word IS the value -> detected.
    Reactive_set(&(*h).base, (pointer) (u32) 99);
    CHECK(Reactive_drain(&(*h).base) == true);
    CHECK(g_changed == 3 && g_new == 99);
    CHECK(ReactiveInt_get(h) == 99);

    // Negative values survive the sign-extension.
    ReactiveInt_set(h, -5);
    CHECK(ReactiveInt_get(h) == -5);

    ReactiveInt_free(h);
}

// Checks boolean reactive round trips and the null getter default.
static fn testBool() {
    printf("[3] bool\n");
    ReactiveBool *b = ReactiveBool(false);
    CHECK(b != nullptr);
    if (!b)
        return;
    CHECK(ReactiveBool_get(b) == false);
    ReactiveBool_set(b, true);
    CHECK(ReactiveBool_get(b) == true);
    ReactiveBool_set(b, false);
    CHECK(ReactiveBool_get(b) == false);
    ReactiveBool_free(b);
    CHECK(ReactiveBool_get(nullptr) == false);
}

// Checks d64 round trips preserve signed zero and NaN payload meaning.
static fn testf64() {
    printf("[4] d64: bit-exact round trip, incl. -0.0 and NaN\n");
    Reactivef64 *d = Reactivef64(1.5);
    CHECK(d != nullptr);
    if (!d)
        return;
    CHECK(Reactivef64_get(d) == 1.5);

    // -0.0 must keep its sign bit (== 0.0 numerically, different bits).
    d64 negZero = -0.0;
    u64 negBits = 0u;
    memcpy(&negBits, &negZero, sizeof(negBits));
    Reactivef64_set(d, negZero);
    CHECK((u64) Reactive_get(&(*d).base) == negBits);
    CHECK(Reactivef64_get(d) == 0.0);

    // NaN round-trips (a numerically unequal value must still survive).
    d64 nan = NAN;
    Reactivef64_set(d, nan);
    CHECK(isnan(Reactivef64_get(d)));

    Reactivef64_set(d, -42.25);
    CHECK(Reactivef64_get(d) == -42.25);
    Reactivef64_free(d);
}

// Checks borrowed string-pointer storage, rebinding notifications, and same-pointer writes.
static fn testString() {
    printf("[5] string: pointer word, rebind is a change\n");
    const u8 *a = (const u8*) "hello";
    const u8 *b = (const u8*) "world";
    ReactiveString *s = ReactiveString(a);
    CHECK(s != nullptr);
    if (!s)
        return;
    CHECK(ReactiveString_get(s) == a);

    g_changed = 0;
    Reactive_addOnChanged(&(*s).base, onChangedInt, nullptr); // counts only
    ReactiveString_set(s, b);
    CHECK(Reactive_drain(&(*s).base) == true);
    CHECK(g_changed == 1);
    CHECK(ReactiveString_get(s) == b);

    // Same pointer again -> onSet fires (a batch happened) but NOT onChanged.
    ReactiveString_set(s, b);
    CHECK(Reactive_drain(&(*s).base) == true);
    CHECK(g_changed == 1); // unchanged: no onChanged on a same-value write

    ReactiveString_free(s);
}

// Checks the object-family constructor chooser, typed object rebind, and merged probable constructor.
static fn testObjectAndProbable() {
    printf("[6] object and probable constructor choosers\n");
    Reactive(Vec2) *emptyVec2 = Reactive(Vec2)();
    Reactive(Vec4) *emptyVec4 = Reactive(Vec4)();
    Reactive(Rectangle) *emptyRectangle = Reactive(Rectangle)();
    Reactive(If64) *emptyIf64 = Reactive(If64)();
    Reactive(Ld128) *emptyLd128 = Reactive(Ld128)();
    CHECK(emptyVec2 && emptyVec4 && emptyRectangle && emptyIf64 && emptyLd128);
    ReactiveVec2_free(emptyVec2); ReactiveVec4_free(emptyVec4);
    ReactiveRectangle_free(emptyRectangle); ReactiveIf64_free(emptyIf64);
    ReactiveLd128_free(emptyLd128);

    Vec3 first = vec3f_3(1.0f, 2.0f, 3.0f);
    Vec3 next = vec3f_3(4.0f, 5.0f, 6.0f);

    Reactive(Vec3) *position = Reactive(Vec3)(&first);
    CHECK(position != nullptr);
    if (position) {
        CHECK(Reactive_addOnChanged(position, onObjectChanged));
        Reactive_set(position, &next);
        CHECK(Reactive_drain(&(*position).base));
        CHECK(g_objectValue == &next && Reactive_get(position) == &next);
        CHECK(Reactive_removeOnChanged(position, onObjectChanged));
        ReactiveVec3_free(position);
    }

    ReactiveProbable(Int) *reward = ReactiveProbable(Int)(25, 1.0f);
    ReactiveProbable(Int) *emptyReward = ReactiveProbable(Int)();
    CHECK(emptyReward && ReactiveProbableInt_getChance(emptyReward) == 0.0f);
    ReactiveProbableInt_free(emptyReward);
    CHECK(reward != nullptr);
    if (reward) {
        CHECK(ReactiveProbableInt_addOnChanged(reward, onProbableChanged));
        ReactiveProbableInt_set(reward, 40, 1.0f);
        CHECK(Reactive_drain(&(*reward).base));
        CHECK(ReactiveProbableInt_get(reward) == 40);
        CHECK(ReactiveProbableInt_getChance(reward) == 1.0f);
        CHECK(g_probableValue == 40);
        CHECK(ReactiveProbableInt_removeOnChanged(reward, onProbableChanged));
        ReactiveProbableInt_free(reward);
    }

}

// Exercises null-safe setters, getters, and destructors for each typed reactive.
static fn testNull() {
    printf("[7] null-safety\n");
    ReactiveInt_set(nullptr, 1);
    CHECK(ReactiveInt_get(nullptr) == 0);
    ReactiveBool_set(nullptr, true);
    Reactivef64_set(nullptr, 1.0);
    ReactiveString_set(nullptr, nullptr);
    CHECK(Reactivef64_get(nullptr) == 0.0);
    CHECK(ReactiveString_get(nullptr) == nullptr);
    ReactiveInt_free(nullptr);
    ReactiveBool_free(nullptr);
    Reactivef64_free(nullptr);
    ReactiveString_free(nullptr);
    CHECK(true);
}

// Runs typed reactive embedding, scalar behavior, notifications, and null-safety cases.
int main() {
    printf("=== Reactive (typed) Test Suite ===\n\n");

    testEmbedFirst();
    testInt();
    testBool();
    testf64();
    testString();
    testObjectAndProbable();
    testNull();

    printf("\n%d checks, %d failed\n", g_checks, g_fail);
    if (g_fail == 0)
        printf("=== Reactive Typed Test Suite: ALL PASS ===\n");
    return g_fail == 0 ? 0 : 1;
}
