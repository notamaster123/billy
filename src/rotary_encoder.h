#pragma once

#include <Arduino.h>

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
};

// KY-040 style encoder: quadrature knob plus a push switch.
//
// Rotation is decoded in an ISR with a transition table rather than by
// sampling one pin on an edge -- the cheap ones bounce badly, and the
// naive approach drops steps and occasionally reports the wrong
// direction. The table only advances on a valid Gray-code transition,
// so contact bounce cancels itself out.
class RotaryEncoder {
public:
    void begin();

    // Detents turned since the last call: negative left, positive
    // right, zero if it has not moved.
    int consumeRotation();

    // A press is only classified once released (short) or once the
    // hold threshold passes (long), so the two never both fire.
    ButtonEvent consumeButton();

private:
    unsigned long pressedAt = 0;
    bool wasDown = false;
    bool longAlreadyFired = false;
    unsigned long lastChange = 0;
};
