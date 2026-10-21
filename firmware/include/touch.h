#pragma once
#include <Arduino.h>

// Capacitive touch, used only as a single button: tap the glass to fire a
// payment in demo mode (handy for filming it), or to skip to the next card
// when it's live.
bool touchBegin();     // true if a touch controller answered on the I2C bus
bool touchTapped();    // true once per press, on the rising edge
