#include "touch.h"
#include "board.h"
#include <Wire.h>

// Register 0x02 is the active-touch count on both the FocalTech (FT3168) and
// CST8xx parts Waveshare fits to these panels, which is all we need -- we
// never read coordinates.
#define REG_TOUCH_COUNT 0x02

static bool present_  = false;
static bool wasDown_  = false;
static uint32_t next_ = 0;

static int readCount() {
  Wire.beginTransmission(ADDR_TOUCH);
  Wire.write(REG_TOUCH_COUNT);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)ADDR_TOUCH, 1) != 1) return -1;
  return Wire.read() & 0x0F;
}

bool touchBegin() {
  Wire.setTimeOut(10);                 // never stall a frame on a dead bus
  Wire.beginTransmission(ADDR_TOUCH);
  present_ = (Wire.endTransmission() == 0);
  return present_;
}

bool touchTapped() {
  if (!present_) return false;
  if ((int32_t)(millis() - next_) < 0) return false;
  next_ = millis() + 50;               // 50ms poll still catches any tap

  const int n = readCount();
  if (n < 0) return false;             // transient bus error: ignore

  const bool down = n > 0;
  const bool tap  = down && !wasDown_;
  wasDown_ = down;
  return tap;
}
