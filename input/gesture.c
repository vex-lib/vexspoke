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


;;GETTER
static inline f32 GestureState_currentRotationRad(const GestureState *s) { return (*s).currentRotationRad; }
;;GETTER
static inline f32 *GestureState_currentRotationRadMut(GestureState *s) { return &(*s).currentRotationRad; }
;;SETTER
static inline fn GestureState_setCurrentRotationRad(GestureState *s, f32 v) { (*s).currentRotationRad = v; }
;;GETTER
static inline f32 GestureState_currentScale(const GestureState *s) { return (*s).currentScale; }
;;GETTER
static inline f32 *GestureState_currentScaleMut(GestureState *s) { return &(*s).currentScale; }
;;SETTER
static inline fn GestureState_setCurrentScale(GestureState *s, f32 v) { (*s).currentScale = v; }
;;GETTER
static inline vec2f GestureState_focalPoint(const GestureState *s) { return (*s).focalPoint; }
;;GETTER
static inline vec2f *GestureState_focalPointMut(GestureState *s) { return &(*s).focalPoint; }
;;SETTER
static inline fn GestureState_setFocalPoint(GestureState *s, vec2f v) { (*s).focalPoint = v; }
;;GETTER
static inline bool GestureState_isPinching(const GestureState *s) { return (*s).isPinching; }
;;GETTER
static inline bool *GestureState_isPinchingMut(GestureState *s) { return &(*s).isPinching; }
;;SETTER
static inline fn GestureState_setIsPinching(GestureState *s, bool v) { (*s).isPinching = v; }
;;GETTER
static inline bool GestureState_isRotating(const GestureState *s) { return (*s).isRotating; }
;;GETTER
static inline bool *GestureState_isRotatingMut(GestureState *s) { return &(*s).isRotating; }
;;SETTER
static inline fn GestureState_setIsRotating(GestureState *s, bool v) { (*s).isRotating = v; }
;;GETTER
static inline vec2f GestureState_scrollVelocity(const GestureState *s) { return (*s).scrollVelocity; }
;;GETTER
static inline vec2f *GestureState_scrollVelocityMut(GestureState *s) { return &(*s).scrollVelocity; }
;;SETTER
static inline fn GestureState_setScrollVelocity(GestureState *s, vec2f v) { (*s).scrollVelocity = v; }

;;GETTER
static inline i32 Point_x(const Point *s) { return (*s).x; }
;;GETTER
static inline i32 *Point_xMut(Point *s) { return &(*s).x; }
;;SETTER
static inline fn Point_setX(Point *s, i32 v) { (*s).x = v; }
;;GETTER
static inline i32 Point_y(const Point *s) { return (*s).y; }
;;GETTER
static inline i32 *Point_yMut(Point *s) { return &(*s).y; }
;;SETTER
static inline fn Point_setY(Point *s, i32 v) { (*s).y = v; }

GestureState GestureState_0() {
    GestureState state;
    memset(&state, 0, sizeof state);
    state.currentScale = 1.0f;
    return state;
}

fn GestureState_processEvent(GestureState *state, GestureEvent event) {
    if (state == nullptr) return;

    GestureState_setFocalPoint(state, event.focalPoint);
    vec2f *velocity = GestureState_scrollVelocityMut(state);
    vec2f delta = event.scrollDelta;

    switch (event.phase) {
        case GESTURE_PHASE_BEGAN:
            if (fabsf(event.magnificationDelta) > 0.0001f) {
                GestureState_setIsPinching(state, true);
            }
            if (fabsf(event.rotationDeltaRad) > 0.0001f) {
                GestureState_setIsRotating(state, true);
            }
            break;

        case GESTURE_PHASE_CHANGED:
            // Accumulate magnification
            GestureState_currentScale(state) *= (1.0f + event.magnificationDelta);
            if (GestureState_currentScale(state) < 0.05f) (*GestureState_currentScaleMut(state)) = 0.05f;
            if (GestureState_currentScale(state) > 50.0f) (*GestureState_currentScaleMut(state)) = 50.0f;

            // Accumulate rotation
            GestureState_setCurrentRotationRad(state, GestureState_currentRotationRad(state) + (event.rotationDeltaRad));

            // Update scroll velocity
            Point_setX(velocity, delta.x);
            Point_setY(velocity, delta.y);
            break;

        case GESTURE_PHASE_ENDED:
        case GESTURE_PHASE_CANCELLED:
            GestureState_setIsPinching(state, false);
            GestureState_setIsRotating(state, false);
            break;

        case GESTURE_PHASE_MOMENTUM:
            // Momentum scroll tick
            Point_setX(velocity, delta.x);
            Point_setY(velocity, delta.y);
            break;

        default:
            break;
    }
}

fn GestureState_tick(GestureState *state, f32 deltaSeconds) {
    if (state == nullptr || deltaSeconds <= 0.0f) return;

    // Decay momentum velocity with smooth exponential friction
    f32 friction = powf(0.1f, deltaSeconds);
    vec2f *velocity = GestureState_scrollVelocityMut(state);
    Point_x(velocity) *= friction;
    Point_y(velocity) *= friction;

    if (fabsf(Point_x(velocity)) < 0.001f) {
        Point_setX(velocity, 0.0f);
    }
    if (fabsf(Point_y(velocity)) < 0.001f) {
        Point_setY(velocity, 0.0f);
    }
}
