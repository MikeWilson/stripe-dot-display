// Host shim: a framebuffer-only stand-in for Arduino_Canvas.
#pragma once
#include "Arduino.h"

#define RGB565_BLACK 0x0000

class Arduino_Canvas {
 public:
  Arduino_Canvas(int16_t w, int16_t h) : w_(w), h_(h) {
    fb_ = (uint16_t *)calloc((size_t)w * h, sizeof(uint16_t));
  }
  uint16_t *getFramebuffer() { return fb_; }
  int16_t width()  const { return w_; }
  int16_t height() const { return h_; }
 private:
  int16_t   w_, h_;
  uint16_t *fb_;
};
