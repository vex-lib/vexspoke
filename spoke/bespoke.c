#include "vex.h"
#include "bespoke.h"

;;DEFINITION
/** The Bespoke bridge hub: the one place where R3 drivers attach themselves
 * through weak extern hooks instead of being linked in. It exists so a driver can
 * arrive late or be hot-replaced without the hub knowing its identity — the first
 * launch resolves what is present, and the periodic check services drivers that
 * appeared since. The hooks are weak and optional, so a build without a given
 * driver still links and simply finds it absent. */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Bespoke (spoke/bespoke.c)
 * ============================================================================
 * Unified Bespoke Bridge Hub. Coordinates R3 drivers (graphics, networking,
 * storage/etc.) via weak extern function hooks. Supports initial launch
 * (bridgeBespoke) and periodic re-check (bridgeBespokeCheck) to service
 * late-starting drivers and restart hot-replaced ones.
 *
 * STRUCT FIELDS (Mirroring spoke/bespoke.h — exactly this file's class):
 * ----------------------------------------------------------------------------
 *   BespokeState graphicsState;
 *   BespokeState networkingState;
 *   BespokeState smthState;
 *   u64 runs;
 *   u64 checks;
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Bespoke()
 * Core Functions:
 *   - bridgeBespoke()
 *   - bridgeBespokeCheck()
 *   - Bespoke_bridge(self)
 *   - Bespoke_check(self)
 * Getters:
 *   - bridgeBespokeGraphicsState()
 *   - bridgeBespokeNetworkingState()
 *   - bridgeBespokeSmthState()
 * ============================================================================
 */


;;GETTER
static inline u64 Bespoke_checks(const Bespoke *s) { return (*s).checks; }
;;SETTER
static inline fn Bespoke_setChecks(Bespoke *s, u64 v) { (*s).checks = v; }
;;GETTER
static inline BespokeState Bespoke_graphicsState(const Bespoke *s) { return (*s).graphicsState; }
;;SETTER
static inline fn Bespoke_setGraphicsState(Bespoke *s, BespokeState v) { (*s).graphicsState = v; }
;;GETTER
static inline BespokeState Bespoke_networkingState(const Bespoke *s) { return (*s).networkingState; }
;;SETTER
static inline fn Bespoke_setNetworkingState(Bespoke *s, BespokeState v) { (*s).networkingState = v; }
;;GETTER
static inline u64 Bespoke_runs(const Bespoke *s) { return (*s).runs; }
;;SETTER
static inline fn Bespoke_setRuns(Bespoke *s, u64 v) { (*s).runs = v; }
;;GETTER
static inline BespokeState Bespoke_smthState(const Bespoke *s) { return (*s).smthState; }
;;SETTER
static inline fn Bespoke_setSmthState(Bespoke *s, BespokeState v) { (*s).smthState = v; }

#if defined(VEX_PLATFORM_APPLE) || defined(VEX_PLATFORM_LINUX)
__attribute__((weak)) bool runGraphics() { return false; }
__attribute__((weak)) BespokeState checkGraphics() { return BESPOKE_ABSENT; }
__attribute__((weak)) bool runNetworking() { return false; }
__attribute__((weak)) BespokeState checkNetworking() { return BESPOKE_ABSENT; }
__attribute__((weak)) bool doSmth() { return false; } // this is just a placeholder
__attribute__((weak)) BespokeState checkSmth() { return BESPOKE_ABSENT; }
#endif

static Bespoke s_defaultBespoke = {
    .graphicsState = BESPOKE_ABSENT,
    .networkingState = BESPOKE_ABSENT,
    .smthState = BESPOKE_ABSENT,
    .runs = 0,
    .checks = 0
};

Bespoke *Bespoke_0() {
    return &s_defaultBespoke;
}

fn Bespoke_bridge(Bespoke *self) {
    if (!self)
        return;

    Bespoke_setRuns(self, Bespoke_runs(self) + 1u);

    // 1. Networking (api-haven)
    if (runNetworking != nullptr) {
        BespokeState state = checkNetworking ? checkNetworking() : BESPOKE_READY;
        if (state == BESPOKE_READY) {
            if (runNetworking())
                Bespoke_setNetworkingState(self, BESPOKE_RUNNING);
            else
                Bespoke_setNetworkingState(self, BESPOKE_LATE);
        } else {
            Bespoke_setNetworkingState(self, state);
        }
    }

    // 2. Custom / doSmth (darkbase-engine or other)
    if (doSmth != nullptr) {
        BespokeState state = checkSmth ? checkSmth() : BESPOKE_READY;
        if (state == BESPOKE_READY) {
            if (doSmth())
                Bespoke_setSmthState(self, BESPOKE_RUNNING);
            else
                Bespoke_setSmthState(self, BESPOKE_LATE);
        } else {
            Bespoke_setSmthState(self, state);
        }
    }

    // 3. Graphics (graphvex)
    if (runGraphics != nullptr) {
        BespokeState state = checkGraphics ? checkGraphics() : BESPOKE_READY;
        if (state == BESPOKE_READY) {
            if (runGraphics())
                Bespoke_setGraphicsState(self, BESPOKE_RUNNING);
            else
                Bespoke_setGraphicsState(self, BESPOKE_LATE);
        } else {
            Bespoke_setGraphicsState(self, state);
        }
    }
}

bool Bespoke_check(Bespoke *self) {
    if (!self)
        return false;

    Bespoke_setChecks(self, Bespoke_checks(self) + 1u);
    bool changed = false;

    // 1. Check Graphics: handle late arrival or hot replacement
    if (runGraphics != nullptr) {
        BespokeState live = checkGraphics ? checkGraphics() : Bespoke_graphicsState(self);
        if (live == BESPOKE_READY || live == BESPOKE_REPLACED) {
            if (runGraphics()) {
                Bespoke_setGraphicsState(self, BESPOKE_RUNNING);
                changed = true;
            }
        } else if (live != Bespoke_graphicsState(self)) {
            Bespoke_setGraphicsState(self, live);
            changed = true;
        }
    }

    // 2. Check Networking
    if (runNetworking != nullptr) {
        BespokeState live = checkNetworking ? checkNetworking() : Bespoke_networkingState(self);
        if (live == BESPOKE_READY || live == BESPOKE_REPLACED) {
            if (runNetworking()) {
                Bespoke_setNetworkingState(self, BESPOKE_RUNNING);
                changed = true;
            }
        } else if (live != Bespoke_networkingState(self)) {
            Bespoke_setNetworkingState(self, live);
            changed = true;
        }
    }

    // 3. Check doSmth
    if (doSmth != nullptr) {
        BespokeState live = checkSmth ? checkSmth() : Bespoke_smthState(self);
        if (live == BESPOKE_READY || live == BESPOKE_REPLACED) {
            if (doSmth()) {
                Bespoke_setSmthState(self, BESPOKE_RUNNING);
                changed = true;
            }
        } else if (live != Bespoke_smthState(self)) {
            Bespoke_setSmthState(self, live);
            changed = true;
        }
    }

    return changed;
}

fn bridgeBespoke() {
    Bespoke_bridge(&s_defaultBespoke);
}

bool bridgeBespokeCheck() {
    return Bespoke_check(&s_defaultBespoke);
}

BespokeState bridgeBespokeGraphicsState() {
    return s_defaultBespoke.graphicsState;
}

BespokeState bridgeBespokeNetworkingState() {
    return s_defaultBespoke.networkingState;
}

BespokeState bridgeBespokeSmthState() {
    return s_defaultBespoke.smthState;
}
