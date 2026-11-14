#include "vex.h"
#include "input/gesture.h"

#include <math.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Gesture
 * ============================================================================
 * Multi-touch trackpad gesture engine: folds raw trackpad events (pinch
 * magnification, two-finger rotation, momentum scroll, swipe directionality)
 * into an accumulated GestureState for high-precision creative viewports.
 * Exists because raw event deltas are noisy; the engine accumulates
 * scale/rotation with clamping and decays scroll velocity with exponential
 * friction. memory: caller-owned GestureState value struct, zero allocation,
 * zero threads. Lifetime: caller owns the `GestureState()` value;
 * processEvent/tick mutate the state in place.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Gesture (input/gesture.c)
 * ============================================================================
 * Processes trackpad pinch magnification, rotation, momentum scrolling,
 * and swipe directionality for high-precision creative viewports.
 *
 * STRUCT FIELDS (Mirroring input/gesture.h):
 * ----------------------------------------------------------------------------
 *   GestureEvent {
 *     GesturePhase phase;            // BEGAN / CHANGED / ENDED / CANCELLED / MOMENTUM
 *     vec2f focalPoint;               // gesture origin (normalized or pixel coords)
 *     f32 magnificationDelta;      // pinch scale delta (+ zoom in, - zoom out)
 *     f32 rotationDeltaRad;        // two-finger rotation (+ CW, - CCW)
 *     vec2f scrollDelta;              // smooth scroll delta (horizontal & vertical)
 *     SwipeDirection swipe;          // discrete multi-finger swipe if detected
 *     u64 timestampUs;          // event timestamp
 *   }
 *   GestureState {
 *     f32 currentScale;            // absolute accumulated zoom scale (default 1.0)
 *     f32 currentRotationRad;      // absolute accumulated rotation
 *     vec2f focalPoint;               // gesture origin
 *     vec2f scrollVelocity;           // inertia velocity decay
 *     bool isPinching;               // pinch in progress
 *     bool isRotating;               // rotation in progress
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - GestureState()
 *   - GestureState_processEvent(state, event)
 *   - GestureState_tick(state, deltaSeconds)
 * ============================================================================
 */

GestureState GestureState_0() {
    GestureState state;
    memset(&state, 0, sizeof state);
    state.currentScale = 1.0f;
    return state;
}

fn GestureState_processEvent(GestureState *state, GestureEvent event) {
    if (state == nullptr) return;

    (*state).focalPoint = event.focalPoint;
    vec2f *velocity = &(*state).scrollVelocity;
    vec2f delta = event.scrollDelta;

    switch (event.phase) {
        case GESTURE_PHASE_BEGAN:
            if (fabsf(event.magnificationDelta) > 0.0001f) {
                (*state).isPinching = true;
            }
            if (fabsf(event.rotationDeltaRad) > 0.0001f) {
                (*state).isRotating = true;
            }
            break;

        case GESTURE_PHASE_CHANGED:
            // Accumulate magnification
            (*state).currentScale *= (1.0f + event.magnificationDelta);
            if ((*state).currentScale < 0.05f) (*state).currentScale = 0.05f;
            if ((*state).currentScale > 50.0f) (*state).currentScale = 50.0f;

            // Accumulate rotation
            (*state).currentRotationRad += event.rotationDeltaRad;

            // Update scroll velocity
            (*velocity).x = delta.x;
            (*velocity).y = delta.y;
            break;

        case GESTURE_PHASE_ENDED:
        case GESTURE_PHASE_CANCELLED:
            (*state).isPinching = false;
            (*state).isRotating = false;
            break;

        case GESTURE_PHASE_MOMENTUM:
            // Momentum scroll tick
            (*velocity).x = delta.x;
            (*velocity).y = delta.y;
            break;

        default:
            break;
    }
}

fn GestureState_tick(GestureState *state, f32 deltaSeconds) {
    if (state == nullptr || deltaSeconds <= 0.0f) return;

    // Decay momentum velocity with smooth exponential friction
    f32 friction = powf(0.1f, deltaSeconds);
    vec2f *velocity = &(*state).scrollVelocity;
    (*velocity).x *= friction;
    (*velocity).y *= friction;

    if (fabsf((*velocity).x) < 0.001f) {
        (*velocity).x = 0.0f;
    }
    if (fabsf((*velocity).y) < 0.001f) {
        (*velocity).y = 0.0f;
    }
}
