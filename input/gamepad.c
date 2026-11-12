#include "vex.h"

#include "input/gamepad.h"

#include <math.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Gamepad
 * ============================================================================
 * Processes digital game controller buttons, dual-motor rumble haptics, and
 * radial circular deadzone vector mapping for analog thumbsticks. GamepadState
 * is a plain value struct (no heap allocation) holding the button bitmask,
 * filtered stick vectors, trigger squeezes, rumble intensities, and the
 * connection flag; callers own it on the stack or in a device slot. The
 * deadzone filter rescales stick vectors from (deadzone..1.0) to (0.0..1.0)
 * and returns the filtered vector by value, keeping the analog model consistent
 * across platforms.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Gamepad (input/gamepad.c)
 * ============================================================================
 * Processes digital game controller buttons, dual-motor rumble haptics,
 * and radial circular deadzone vector mapping for analog thumbsticks.
 *
 * STRUCT FIELDS (Mirroring input/gamepad.h):
 * ----------------------------------------------------------------------------
 *   GamepadState {
 *     u32 buttons;       // bitmask of active pressed buttons
 *     vec2f     leftStick;     // filtered deadzone vector [-1.0, +1.0]
 *     vec2f     rightStick;    // filtered deadzone vector [-1.0, +1.0]
 *     f32    leftTrigger;   // normalized trigger squeeze [0.0, 1.0]
 *     f32    rightTrigger;  // normalized trigger squeeze [0.0, 1.0]
 *     f32    rumbleLowFreq; // low frequency heavy rumble [0.0, 1.0]
 *     f32    rumbleHighFreq;// high frequency sharp rumble [0.0, 1.0]
 *     bool     isConnected;
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - GamepadState()
 *   - GamepadState_isDown(state, buttonMask)
 *   - Gamepad_applyCircularDeadzone(rawStick, deadzone)
 * ============================================================================
 */

GamepadState GamepadState_0(void) {
    GamepadState state;
    memset(&state, 0, sizeof state);
    state.isConnected = false;
    return state;
}

bool GamepadState_isDown(const GamepadState *state, u32 buttonMask) {
    if (state == nullptr) return false;
    return ((*state).buttons & buttonMask) == buttonMask;
}

vec2f Gamepad_applyCircularDeadzone(vec2f rawStick, f32 deadzone) {
    if (deadzone <= 0.0f)
        return rawStick;
    if (deadzone >= 0.99f) {
        deadzone = 0.99f;
    }

    f32 mag = sqrtf(rawStick.x * rawStick.x +
                      rawStick.y * rawStick.y);

    vec2f filtered;
    if (mag <= deadzone) {
        filtered = vec2f_2(0.0f, 0.0f);
        return filtered;
    }

    // Rescale vector smoothly from (deadzone..1.0) to (0.0..1.0)
    f32 normalizedMag = (mag - deadzone) / (1.0f - deadzone);
    if (normalizedMag > 1.0f) {
        normalizedMag = 1.0f;
    }

    f32 scale = normalizedMag / mag;
    filtered = vec2f_2(rawStick.x * scale, rawStick.y * scale);
    return filtered;
}
