#include "rotary_encoder.h"

#include "config.h"

namespace {

// Indexed by (previous two bits << 2) | current two bits. Non-zero
// only for the four valid Gray-code transitions; bounce produces
// invalid or reversing pairs that sum back to zero.
const int8_t TRANSITIONS[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0,
};

volatile uint8_t gState = 0;
volatile int8_t gSubSteps = 0;   // quarter-steps within one detent
volatile int32_t gDetents = 0;

void IRAM_ATTR onEncoderEdge() {
    uint8_t bits = (digitalRead(ENCODER_CLK_PIN) << 1) | digitalRead(ENCODER_DT_PIN);
    gState = ((gState << 2) | bits) & 0x0F;

    int8_t step = TRANSITIONS[gState];
    if (step == 0) {
        return;
    }

    gSubSteps += step;

    // A detent is four quarter-steps. Only emitting on a complete
    // detent is what stops one click reporting as two or three.
    if (gSubSteps >= 4) {
        gDetents++;
        gSubSteps = 0;
    } else if (gSubSteps <= -4) {
        gDetents--;
        gSubSteps = 0;
    }
}

}  // namespace

void RotaryEncoder::begin() {
    pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
    pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

    gState = (digitalRead(ENCODER_CLK_PIN) << 1) | digitalRead(ENCODER_DT_PIN);
    gSubSteps = 0;
    gDetents = 0;

    attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), onEncoderEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_DT_PIN), onEncoderEdge, CHANGE);
}

int RotaryEncoder::consumeRotation() {
    noInterrupts();
    int32_t detents = gDetents;
    gDetents = 0;
    interrupts();
    return static_cast<int>(detents);
}

ButtonEvent RotaryEncoder::consumeButton() {
    bool down = digitalRead(ENCODER_SW_PIN) == LOW;  // active low
    unsigned long now = millis();

    if (down != wasDown) {
        if (now - lastChange < ENCODER_DEBOUNCE_MS) {
            return ButtonEvent::NONE;
        }
        lastChange = now;
        wasDown = down;

        if (down) {
            pressedAt = now;
            longAlreadyFired = false;
        } else if (!longAlreadyFired) {
            return ButtonEvent::SHORT_PRESS;
        }
        return ButtonEvent::NONE;
    }

    // Fire the long press while still held, so it feels immediate
    // rather than waiting for release.
    if (down && !longAlreadyFired && now - pressedAt >= ENCODER_LONG_PRESS_MS) {
        longAlreadyFired = true;
        return ButtonEvent::LONG_PRESS;
    }

    return ButtonEvent::NONE;
}
