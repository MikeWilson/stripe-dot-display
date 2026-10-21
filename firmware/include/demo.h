#pragma once
#include "state.h"

// Self-contained demo: plausible stats and a payment every few seconds, with
// no WiFi and no relay. This is what DEMO_MODE in config.h selects, and it's
// the fastest way to confirm a new board's panel, pin map and framerate.
void demoBegin(Stats &s);
void demoTick (Stats &s);
bool demoPayment(PaymentEvent &out);   // true on the frames a fake sale fires
